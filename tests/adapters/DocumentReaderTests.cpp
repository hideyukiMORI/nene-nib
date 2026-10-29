// Issue #993（ADR 0159 の決定 2・3・5・6・ADR 0141 の追補 10）: 開く道の読みの本物
// （Win32DocumentReader）を本物のファイルと worker 1 本で測る。64 MiB を超える見本は worker の塊の
// 仕事で読まれ、進み具合が塊ごとに届き、揃った本文の長さと行数が見本と一致する。取消の後は終わりが
// 届かない。64 MiB 以下は呼んだスレッドで揃う。見本はビルドディレクトリの直下に固定の名前で書き、
// 冒頭で前回の残りを消して終わりに消す。時刻は読まない（Sleep・時計・タイムアウトの待ちを書かない）。
// 待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest の制限時間で落ちる。
#include "DocumentReadFinished.hpp"
#include "DocumentReadOutcome.hpp"
#include "DocumentReadProgress.hpp"
#include "DocumentReadRequest.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "TextEncoding.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32DocumentReader.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
namespace adapters = nenenib::adapters::win32;
using Arrivals = std::counting_semaphore<>;
using Outcomes = std::vector<app::DocumentReadOutcome>;

constexpr wchar_t large_name[] = L"nib-document-reader-large.txt";
constexpr char large_path[] = "nib-document-reader-large.txt";
constexpr wchar_t small_name[] = L"nib-document-reader-small.txt";
constexpr char small_path[] = "nib-document-reader-small.txt";
// 64 バイトの固定の行（LF）。65 MiB は 64 MiB の塊 1 つと残りの 1 MiB に分かれる。
constexpr std::string_view sample_line =
    "nib document reader sample line 0123456789 abcdefghijklmnopqrst\n";
constexpr std::size_t large_bytes = std::size_t{65} * 1024 * 1024;
constexpr std::size_t large_lines = large_bytes / 64;
constexpr std::size_t no_limit = std::size_t{1} << 40U;

static_assert(sample_line.size() == 64);

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

void write_sample(const wchar_t *name, std::size_t lines)
{
    std::string body;
    body.reserve(lines * sample_line.size());
    for (std::size_t line = 0; line < lines; ++line)
    {
        body.append(sample_line);
    }
    const HANDLE file =
        CreateFileW(name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a sample file is created");
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    expect(WriteFile(file, body.data(), static_cast<DWORD>(body.size()), &written, nullptr) != 0 &&
               written == body.size(),
           "the sample file is written whole");
    CloseHandle(file);
}

[[nodiscard]] bool finished_for(const Outcomes &outcomes, std::uint64_t ticket)
{
    for (const auto &outcome : outcomes)
    {
        const auto *finished = std::get_if<app::DocumentReadFinished>(&outcome);
        if (finished != nullptr && finished->ticket == ticket)
        {
            return true;
        }
    }
    return false;
}

// 合図を待っては collect し、その券の終わりが届くまでためる。
Outcomes until_finished(adapters::Win32DocumentReader &reads, Arrivals &arrived,
                        std::uint64_t ticket)
{
    Outcomes seen;
    while (!finished_for(seen, ticket))
    {
        arrived.acquire();
        for (auto &outcome : reads.collect())
        {
            seen.push_back(std::move(outcome));
        }
    }
    return seen;
}

// worker の列の今の後ろまで流れたことを、自分の仕事の合図で待つ。
void drain(adapters::Win32Worker &worker)
{
    Arrivals reached{0};
    worker.post([&reached](const std::stop_token &) { reached.release(); });
    reached.acquire();
}

void verify_large_on_worker()
{
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32CodePageAdapter code_pages;
    adapters::Win32DocumentReader reads(worker, files, code_pages);
    Arrivals arrived{0};
    reads.bind([&arrived] { arrived.release(); });
    const auto asked = reads.request(app::DocumentReadRequest{path_of(large_path), 7, no_limit});
    expect(!asked.has_value(), "a file over 64 MiB is read on the worker");
    const auto seen = until_finished(reads, arrived, 7);
    std::size_t progress = 0;
    std::uint64_t last_read = 0;
    for (const auto &outcome : seen)
    {
        const auto *step = std::get_if<app::DocumentReadProgress>(&outcome);
        if (step != nullptr)
        {
            ++progress;
            expect(step->ticket == 7 && step->total_bytes == large_bytes &&
                       step->read_bytes > last_read,
                   "progress goes forward against the file size");
            last_read = step->read_bytes;
        }
    }
    expect(progress >= 2 && last_read == large_bytes, "each chunk reports progress");
    const auto &finished = std::get<app::DocumentReadFinished>(seen.back());
    expect(finished.document.has_value(), "the large file opens");
    if (finished.document.has_value())
    {
        const auto &document = finished.document.value();
        expect(document.encoding == core::TextEncoding::utf8 &&
                   document.text.size_bytes() == large_bytes &&
                   document.text.line_count() == large_lines + 1,
               "the body has the sample's length and lines");
    }
}

void verify_cancelled_on_worker()
{
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32CodePageAdapter code_pages;
    adapters::Win32DocumentReader reads(worker, files, code_pages);
    Arrivals arrived{0};
    reads.bind([&arrived] { arrived.release(); });
    const auto asked = reads.request(app::DocumentReadRequest{path_of(large_path), 9, no_limit});
    expect(!asked.has_value(), "the cancelled read starts on the worker");
    reads.cancel(9);
    // 走っていた塊が積み直した次の塊も流れ終わるまで、列の後ろを 2 回待つ。
    drain(worker);
    drain(worker);
    expect(!finished_for(reads.collect(), 9), "a cancelled read never finishes");
    // 取り消した読みはハンドルを手放しているので、同じファイルを書き込みで（共有なしで）開ける。
    const HANDLE writer = CreateFileW(large_name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(writer != INVALID_HANDLE_VALUE, "a cancelled read releases the file for writing");
    if (writer != INVALID_HANDLE_VALUE)
    {
        CloseHandle(writer);
    }
}

void verify_small_synchronous()
{
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32CodePageAdapter code_pages;
    adapters::Win32DocumentReader reads(worker, files, code_pages);
    const auto asked = reads.request(app::DocumentReadRequest{path_of(small_path), 3, no_limit});
    expect(asked.has_value() && std::holds_alternative<app::DocumentReadFinished>(asked.value()),
           "a file of 64 MiB or less is read on the calling thread");
    if (asked.has_value())
    {
        const auto &finished = std::get<app::DocumentReadFinished>(asked.value());
        expect(finished.ticket == 3 && finished.document.has_value() &&
                   finished.document.value().text.line_count() == 4 + 1,
               "the small file opens with its lines");
    }
    expect(!worker.started(), "a small file does not wake the worker");
    const auto missing = reads.request(
        app::DocumentReadRequest{path_of("nib-document-reader-none.txt"), 4, no_limit});
    expect(missing.has_value() &&
               !std::get<app::DocumentReadFinished>(missing.value()).document.has_value() &&
               std::get<app::DocumentReadFinished>(missing.value()).document.error() ==
                   app::FileFailure::not_found,
           "a missing file fails on the calling thread");
}
} // namespace

int main()
{
    DeleteFileW(large_name);
    DeleteFileW(small_name);
    write_sample(large_name, large_lines);
    write_sample(small_name, 4);
    verify_small_synchronous();
    verify_large_on_worker();
    verify_cancelled_on_worker();
    expect(DeleteFileW(large_name) != 0 && DeleteFileW(small_name) != 0,
           "the sample files are removed");
    std::printf("Document reader adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
