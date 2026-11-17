// #1793 段 P2（ADR 0189 の決定 4・8）: プレビューの窓の adapter（Win32PreviewAdapter）を本物の
// ファイル・ワーカー 1 本・本物の文字コードの変換で測る。一時フォルダは temporary_test_directory
// （#1103）の中。窓の形（先頭から・当たりの前 7 行から・最後の空の行・CRLF・Shift_JIS・UTF-16）と、
// 作れない理由（16 MiB 超・NUL・無いファイル・フォルダ）と、新しい要求が古い要求の結果を出させない
// ことを確かめる。時刻は読まない。待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest の
// 制限時間で落ちる。
#include "FilePath.hpp"
#include "FinderPreview.hpp"
#include "Language.hpp"
#include "NativePath.hpp"
#include "PreviewAnchor.hpp"
#include "PreviewBatch.hpp"
#include "PreviewFailure.hpp"
#include "PreviewRequest.hpp"
#include "ProjectSearchRules.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Utf16.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32PreviewAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::PreviewAnchor;
using nenenib::application::PreviewBatch;
using nenenib::application::PreviewFailure;
using nenenib::application::PreviewRequest;
using Arrivals = std::counting_semaphore<>;

constexpr std::size_t mebibyte = std::size_t{1} << 20U;

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

std::string &folder()
{
    static std::string root;
    return root;
}

std::string inside(std::string_view name)
{
    return folder() + "\\" + std::string(name);
}

void write_file(const std::string &path, std::string_view bytes)
{
    const auto native = adapters::native_path(core::to_utf16(path).value_or(std::wstring{}));
    const HANDLE file = CreateFileW(native.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a temporary file is created");
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    std::size_t written = 0;
    while (written < bytes.size())
    {
        DWORD done = 0;
        const auto part = std::min<std::size_t>(bytes.size() - written, mebibyte);
        if (WriteFile(file, bytes.data() + written, static_cast<DWORD>(part), &done, nullptr) == 0)
        {
            break;
        }
        written += done;
    }
    expect(written == bytes.size(), "a temporary file is written whole");
    CloseHandle(file);
}

// 1 行目から count 行目まで「line N」（区切りは separator・最後も改行で終える）。
std::string numbered(std::size_t count, std::string_view separator)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += "line " + std::to_string(line);
        text += separator;
    }
    return text;
}

// 1 つの要求を流して、届いた batch を返す（合図を待つ）。
class Reader final
{
  public:
    Reader() : previews_(worker_, code_pages_)
    {
        previews_.bind([this] { arrived_.release(); });
    }

    [[nodiscard]] PreviewBatch read(std::string_view name, std::optional<PreviewAnchor> anchor,
                                    core::Language language = core::Language::plain)
    {
        previews_.request(PreviewRequest{core::FilePath::parse(inside(name)).value(), anchor,
                                         language, ++tickets_});
        arrived_.acquire();
        auto batches = previews_.collect();
        expect(batches.size() == 1 && batches.front().ticket == tickets_,
               "one request gives one batch with its ticket");
        return batches.empty() ? PreviewBatch{0, std::unexpected(PreviewFailure::unreadable)}
                               : std::move(batches.front());
    }

    // 2 つ続けて頼み、後の要求の batch だけが届くこと。先に worker を門の仕事で塞ぎ、2 つの要求を
    // 積んでから門を開ける（前の読みが走る前に新しい世代になる）。
    [[nodiscard]] std::vector<PreviewBatch> read_twice(std::string_view first,
                                                       std::string_view second)
    {
        Arrivals gate{0};
        worker_.post([&gate](const std::stop_token &) { gate.acquire(); });
        previews_.request(PreviewRequest{core::FilePath::parse(inside(first)).value(), std::nullopt,
                                         core::Language::plain, ++tickets_});
        previews_.request(PreviewRequest{core::FilePath::parse(inside(second)).value(),
                                         std::nullopt, core::Language::plain, ++tickets_});
        gate.release();
        arrived_.acquire();
        return previews_.collect();
    }

    // 合図を受けたまま collect せずに取り消し、次の collect で棚を空にすると、次の要求の合図がまた
    // 出る（controller が面を閉じた後に棚を空にする形・V-1794 の必須 1）。
    [[nodiscard]] bool drained_then_signalled(std::string_view name)
    {
        previews_.request(PreviewRequest{core::FilePath::parse(inside(name)).value(), std::nullopt,
                                         core::Language::plain, ++tickets_});
        arrived_.acquire();
        previews_.cancel(tickets_);
        const auto stale = previews_.collect();
        const bool emptied = previews_.collect().empty();
        previews_.request(PreviewRequest{core::FilePath::parse(inside(name)).value(), std::nullopt,
                                         core::Language::plain, ++tickets_});
        arrived_.acquire();
        const auto fresh = previews_.collect();
        return stale.size() == 1 && emptied && fresh.size() == 1 &&
               fresh.front().ticket == tickets_;
    }

    [[nodiscard]] std::uint64_t last() const noexcept
    {
        return tickets_;
    }

  private:
    adapters::Win32Worker worker_;
    adapters::Win32CodePageAdapter code_pages_;
    adapters::Win32PreviewAdapter previews_;
    Arrivals arrived_{0};
    std::uint64_t tickets_ = 0;
};

[[nodiscard]] bool failed_with(const PreviewBatch &batch, PreviewFailure failure)
{
    return !batch.lines.has_value() && batch.lines.error() == failure;
}

void verify_windows(Reader &reader)
{
    const auto head = reader.read("lf.txt", std::nullopt);
    expect(head.lines.has_value() && head.lines.value().size() == core::finder_preview_line_limit &&
               head.lines.value().front().text == "line 1" &&
               head.lines.value().back().text == "line 40",
           "a file is read from the top for up to 40 lines");
    const auto middle =
        reader.read("lf.txt", PreviewAnchor{core::LineNumber{50}, 0, 4}, core::Language::plain);
    expect(middle.lines.has_value() && middle.lines.value().front().number.value == 43 &&
               middle.lines.value().at(7).hit && middle.lines.value().at(7).match_end == 4,
           "an anchored window starts seven lines above the hit and marks it");
    const auto last = reader.read("short.txt", std::nullopt);
    expect(last.lines.has_value() && last.lines.value().size() == 3 &&
               last.lines.value().back().text == "line 3",
           "the empty line after the final line break is not shown");
    const auto crlf = reader.read("crlf.txt", std::nullopt);
    expect(crlf.lines.has_value() && crlf.lines.value().size() == 2 &&
               crlf.lines.value().front().text == "line 1",
           "a CRLF file has no carriage return in its lines");
    const auto sjis = reader.read("sjis.txt", std::nullopt);
    expect(sjis.lines.has_value() && !sjis.lines.value().empty() &&
               sjis.lines.value().front().text == "\xE6\x97\xA5\xE6\x9C\xAC",
           "a Shift_JIS file is decoded like the open path");
    const auto wide = reader.read("utf16.txt", std::nullopt);
    expect(wide.lines.has_value() && !wide.lines.value().empty() &&
               wide.lines.value().front().text == "ab",
           "a UTF-16 file with a BOM is read although it holds NUL bytes");
    const auto colored = reader.read("code.cpp", std::nullopt, core::Language::cpp);
    expect(colored.lines.has_value() && !colored.lines.value().empty() &&
               !colored.lines.value().front().colors.empty(),
           "the window carries syntax colors for its language");
}

void verify_failures(Reader &reader)
{
    expect(failed_with(reader.read("large.txt", std::nullopt), PreviewFailure::too_large),
           "a file over 16 MiB is not read");
    expect(failed_with(reader.read("nul.bin", std::nullopt), PreviewFailure::binary),
           "a file with NUL is not a text preview");
    expect(failed_with(reader.read("missing.txt", std::nullopt), PreviewFailure::unreadable),
           "a missing file cannot be read");
    expect(failed_with(reader.read("sub", std::nullopt), PreviewFailure::unreadable),
           "a folder is not read");
    const auto twice = reader.read_twice("lf.txt", "short.txt");
    expect(twice.size() == 1 && twice.front().ticket == reader.last() &&
               twice.front().lines.has_value() && twice.front().lines.value().size() == 3,
           "a newer request keeps the older one from giving a batch");
    expect(reader.drained_then_signalled("short.txt"),
           "after the shelf is drained the next request signals again");
}

void make_files()
{
    write_file(inside("lf.txt"), numbered(60, "\n"));
    write_file(inside("short.txt"), numbered(3, "\n"));
    write_file(inside("crlf.txt"), numbered(2, "\r\n"));
    write_file(inside("sjis.txt"), "\x93\xFA\x96\x7B\n");
    write_file(inside("utf16.txt"), std::string("\xFF\xFE"
                                                "a\0b\0\n\0",
                                                8));
    write_file(inside("code.cpp"), "return 0; // done\n");
    write_file(inside("nul.bin"), std::string("ab\0cd", 5));
    write_file(inside("large.txt"),
               std::string(static_cast<std::size_t>(core::project_search_file_limit) + 1, 'a'));
    const auto sub = adapters::native_path(core::to_utf16(inside("sub")).value_or(std::wstring{}));
    expect(CreateDirectoryW(sub.c_str(), nullptr) != 0, "a temporary folder is created");
}

int run_tests()
{
    wchar_t full[MAX_PATH]{};
    if (GetFullPathNameW(L"nib-preview", MAX_PATH, full, nullptr) == 0)
    {
        std::fprintf(stderr, "FAIL: the folder has no full path\n");
        return 1;
    }
    folder() = core::to_utf8(std::wstring_view(full)).value_or(std::string{});
    const auto native = adapters::native_path(full);
    expect(CreateDirectoryW(native.c_str(), nullptr) != 0, "the preview folder is created");
    make_files();
    Reader reader;
    verify_windows(reader);
    verify_failures(reader);
    std::printf("Preview adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"preview");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made (error %lu)\n",
                     static_cast<unsigned long>(directory.error()));
        return 1;
    }
    const int result = run_tests();
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
