// Issue #1290: 開く道の worker の塊の読み（DocumentChunkRead・ADR 0159 の決定 2・3）を、塊の境が
// どこに来ても一括の読みと同じになるかで見る性質の試験。固定 seed の決定的な生成器で本文（UTF-8・
// UTF-8 BOM・Shift_JIS・UTF-16 LE / BE・BOM の欠片・でたらめなバイト列。改行は CRLF・LF・lone CR を
// 混ぜ、不正なバイト・途中で切れた多バイト・孤立したサロゲート・奇数長も入れる）を作って本物の
// ファイルに書き、次の 2 本で読んで比べる。
//   一括: Win32DocumentReader の既定（64 MiB 以下は呼んだスレッドで SynchronousDocumentReader）。
//   塊:   同期の上限を 0 にした Win32DocumentReader（1 バイト以上は worker の塊の仕事）に、塊の
//         大きさ k を渡す。k は 1・2・3 と、最初の境が多バイトの途中・CR と LF の間・BOM の途中に
//         来る値（その位置そのもの）と、でたらめな値。
// 不変条件は「分割しても同じ」: 成否・失敗の種類・本文のバイト列・文字コード・改行の形・行数が
// 一括と同じで、塊の読みは ceil(大きさ / k) 回の進み具合を届けて最後に読んだ量がファイルの大きさ。
// CP932 は本物の Win32CodePageAdapter。置き場は temporary_test_directory（#1103）の中の固定の名前。
// 時刻は読まない（待つのは合図の受け皿のセマフォだけで、止まらなければ CTest の制限時間で落ちる）。
// 生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。回数は Debug で 1 秒前後の値。
// 製品の 1 行を手元で壊して落ちることを確かめた（#1290・commit しない）。
#include "DocumentByteLimit.hpp"
#include "DocumentReadFinished.hpp"
#include "DocumentReadOutcome.hpp"
#include "DocumentReadProgress.hpp"
#include "DocumentReadRequest.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "LineEnding.hpp"
#include "OpenedDocument.hpp"
#include "TemporaryTestDirectory.hpp"
#include "TextEncoding.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32DocumentReader.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <random>
#include <semaphore>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
namespace adapters = nenenib::adapters::win32;
using Arrivals = std::counting_semaphore<>;
using Outcomes = std::vector<app::DocumentReadOutcome>;
using Opened = std::expected<app::OpenedDocument, app::FileFailure>;
using Random = std::mt19937_64;
using Make = std::string (*)(Random &);

constexpr std::size_t rounds = 1U;
constexpr std::size_t chunk_inputs = 84U * rounds;
constexpr std::size_t piece_limit = 60U;
constexpr std::uint64_t chunk_seed = 12901U;
constexpr wchar_t sample_name[] = L"nib-document-chunk-sample.txt";
constexpr char sample_path[] = "nib-document-chunk-sample.txt";

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

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] char byte_of(std::size_t value)
{
    return static_cast<char>(static_cast<unsigned char>(value & 0xFFU));
}

// --- 本文の生成器 ---

// UTF-8 の欠片。改行 3 種・2〜4 バイトの文字・結合文字・不正なバイト・途中で切れた列。
constexpr std::array<std::string_view, 16> utf8_pieces{
    {"a", "word ", "\t", "\n", "\r\n", "\r", "\xC3\xA9", "\xE6\x97\xA5\xE6\x9C\xAC", "\xE3\x81\x8B",
     "\xF0\x9F\x98\x80", "e\xCC\x81", "\xE2\x80\x8B", "\x80", "\xFF", "\xE3\x81", "\xC2\x85"}};

// CP932 の欠片。改行 3 種・2 バイトの文字（2 バイト目が ASCII の範囲のものを含む）・半角カナ・
// 不正なもの（後ろの無い先頭バイト・0x80・0xA0・0xFD）。
constexpr std::array<std::string_view, 16> cp932_pieces{
    {"a", "text ", "\n", "\r\n", "\r", "\x93\xFA\x96\x7B", "\x82\xA0", "\x83\x5C", "\x81\x40",
     "\xB1\xB2", "\x88\x9F", "\xE0\x40", "\x81", "\x80", "\xA0", "\xFD"}};

// UTF-16 の単位（ホストの値）。改行 3 種・BMP・サロゲートの対・孤立したサロゲート。
constexpr std::array<std::u16string_view, 12> utf16_pieces{{u"a", u"word ", u"\n", u"\r\n", u"\r",
                                                            u"日本", u"あ", u"\U0001F600", u"é",
                                                            u"﻿", u"\xD800", u"\xDC00"}};

[[nodiscard]] std::string joined(Random &random, std::span<const std::string_view> pieces)
{
    std::string text;
    const std::size_t count = below(random, piece_limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        text.append(pieces[below(random, pieces.size())]);
    }
    return text;
}

[[nodiscard]] std::string utf8_text(Random &random)
{
    return joined(random, utf8_pieces);
}

[[nodiscard]] std::string utf8_bom_text(Random &random)
{
    return "\xEF\xBB\xBF" + joined(random, utf8_pieces);
}

[[nodiscard]] std::string cp932_text(Random &random)
{
    return joined(random, cp932_pieces);
}

void append_units(std::string &bytes, std::u16string_view units, bool little)
{
    for (const char16_t unit : units)
    {
        const auto value = static_cast<std::size_t>(unit);
        bytes.push_back(byte_of(little ? value : value >> 8U));
        bytes.push_back(byte_of(little ? value >> 8U : value));
    }
}

[[nodiscard]] std::string utf16_text(Random &random, bool little)
{
    std::string bytes = little ? std::string("\xFF\xFE") : std::string("\xFE\xFF");
    const std::size_t count = below(random, piece_limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        append_units(bytes, utf16_pieces.at(below(random, utf16_pieces.size())), little);
    }
    // ときどき奇数長にする。
    if (below(random, 6U) == 0U)
    {
        bytes.push_back('A');
    }
    return bytes;
}

[[nodiscard]] std::string utf16_le_text(Random &random)
{
    return utf16_text(random, true);
}

[[nodiscard]] std::string utf16_be_text(Random &random)
{
    return utf16_text(random, false);
}

// BOM の欠片（UTF-8 の 2 バイトまで・UTF-16 の 1 バイト・UTF-32 の BOM）に本文が続く。
[[nodiscard]] std::string bom_fragment_text(Random &random)
{
    constexpr std::array<std::string_view, 5> fragments{
        {"\xEF", "\xEF\xBB", "\xFF", "\xFE", std::string_view("\xFF\xFE\x00\x00", 4)}};
    return std::string(fragments.at(below(random, fragments.size()))) + joined(random, utf8_pieces);
}

[[nodiscard]] std::string random_text(Random &random)
{
    std::string bytes;
    const std::size_t count = below(random, 200U);
    for (std::size_t index = 0; index < count; ++index)
    {
        bytes.push_back(byte_of(below(random, 256U)));
    }
    return bytes;
}

constexpr std::array<Make, 7> makers{{utf8_text, utf8_bom_text, cp932_text, utf16_le_text,
                                      utf16_be_text, bom_fragment_text, random_text}};

// 最初の塊の境が意味のある所に来る大きさ: 多バイトの途中（続きのバイトの前）・CR と LF の間・
// BOM の途中（1 と 2）・UTF-16 の単位の途中（奇数の位置）。
[[nodiscard]] std::vector<std::size_t> telling_cuts(std::string_view bytes)
{
    std::vector<std::size_t> cuts;
    for (std::size_t at = 1; at < bytes.size(); ++at)
    {
        const auto value = static_cast<unsigned char>(bytes[at]);
        const bool continuation = value >= 0x80U && value <= 0xBFU;
        const bool line_feed = bytes[at] == '\n' && bytes[at - 1U] == '\r';
        const bool split_unit = (at % 2U == 1U) && bytes[at] == '\0';
        if (continuation || line_feed || split_unit || at <= 2U)
        {
            cuts.push_back(at);
        }
    }
    return cuts;
}

[[nodiscard]] std::vector<std::size_t> chunk_sizes(Random &random, std::string_view bytes)
{
    std::vector<std::size_t> sizes{1U, 2U, 3U};
    const auto cuts = telling_cuts(bytes);
    for (std::size_t index = 0; index < 3U && !cuts.empty(); ++index)
    {
        sizes.push_back(cuts.at(below(random, cuts.size())));
    }
    sizes.push_back(1U + below(random, bytes.size() + 1U));
    return sizes;
}

// --- 読む ---

void write_sample(std::string_view bytes)
{
    const HANDLE file = CreateFileW(sample_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a sample file is created");
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    expect(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) !=
                   0 &&
               written == bytes.size(),
           "the sample file is written whole");
    CloseHandle(file);
}

[[nodiscard]] app::DocumentReadRequest request_of(std::uint64_t ticket)
{
    return app::DocumentReadRequest{core::FilePath::parse(sample_path).value(), ticket,
                                    core::maximum_document_bytes};
}

[[nodiscard]] Opened bulk_read(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files,
                               adapters::Win32CodePageAdapter &code_pages)
{
    adapters::Win32DocumentReader reads(worker, files, code_pages);
    auto asked = reads.request(request_of(1));
    expect(asked.has_value(), "a small sample is read on the calling thread");
    if (!asked.has_value())
    {
        return std::unexpected(app::FileFailure::unreadable);
    }
    return std::get<app::DocumentReadFinished>(std::move(asked).value()).document;
}

// 塊の読み: 終わりと、届いた進み具合の回数と、最後に読んだ量。
using Chunked = std::pair<Opened, std::pair<std::size_t, std::uint64_t>>;
using Progress = std::pair<std::size_t, std::uint64_t>;

// 今たまっている結果を読み、進み具合を数える。終わりが届いていればそれを返す。
[[nodiscard]] std::optional<Opened> collected(adapters::Win32DocumentReader &reads,
                                              Progress &progress)
{
    for (auto &outcome : reads.collect())
    {
        if (auto *step = std::get_if<app::DocumentReadProgress>(&outcome))
        {
            ++progress.first;
            progress.second = step->read_bytes;
            continue;
        }
        return std::get<app::DocumentReadFinished>(std::move(outcome)).document;
    }
    return std::nullopt;
}

[[nodiscard]] Chunked chunked_read(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files,
                                   adapters::Win32CodePageAdapter &code_pages, std::size_t chunk)
{
    adapters::Win32DocumentReader reads(worker, files, code_pages, 0, chunk);
    Arrivals arrived{0};
    reads.bind([&arrived] { arrived.release(); });
    auto asked = reads.request(request_of(2));
    if (asked.has_value())
    {
        // 0 バイトのファイルは大きさが同期の上限（0）以下なので呼んだスレッドで揃う。
        return {std::get<app::DocumentReadFinished>(std::move(asked).value()).document, {0U, 0U}};
    }
    Progress progress{0U, 0U};
    while (true)
    {
        arrived.acquire();
        auto finished = collected(reads, progress);
        if (finished.has_value())
        {
            return {std::move(finished).value(), progress};
        }
    }
}

[[nodiscard]] bool same_opened(const Opened &left, const Opened &right)
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    if (!left.has_value())
    {
        return left.error() == right.error();
    }
    const auto &one = left.value();
    const auto &other = right.value();
    return one.encoding == other.encoding && one.text.line_ending() == other.text.line_ending() &&
           one.text.line_count() == other.text.line_count() && one.text.text() == other.text.text();
}

// 読みに渡す 3 つの本物（worker・ファイル・CP932）。
struct ChunkPorts
{
    adapters::Win32Worker &worker;
    adapters::Win32FileAdapter &files;
    adapters::Win32CodePageAdapter &code_pages;
};

// 1 つの本文を全部の塊の大きさで読み、最初に見つけた違いの説明を返す（無ければ空）。
[[nodiscard]] std::string_view input_violation(Random &random, std::string_view bytes,
                                               const ChunkPorts &ports)
{
    auto &[worker, files, code_pages] = ports;
    write_sample(bytes);
    const Opened bulk = bulk_read(worker, files, code_pages);
    for (const std::size_t chunk : chunk_sizes(random, bytes))
    {
        const auto [opened, progress] = chunked_read(worker, files, code_pages, chunk);
        if (!same_opened(bulk, opened))
        {
            std::fprintf(stderr, "chunk %zu of %zu bytes differs from the whole read\n", chunk,
                         bytes.size());
            return "a chunked read opens the same document or fails the same way as a whole read";
        }
        const std::size_t expected = bytes.empty() ? 0U : (bytes.size() + chunk - 1U) / chunk;
        if (progress.first != expected || progress.second != bytes.size())
        {
            return "a chunked read reports one progress per chunk up to the file size";
        }
    }
    return {};
}

void verify_known_cuts(adapters::Win32Worker &worker, adapters::Win32FileAdapter &files,
                       adapters::Win32CodePageAdapter &code_pages)
{
    // BOM の途中・CRLF の間・3 バイトの文字の途中・CP932 の 2 バイトの間・UTF-16 の単位の間。
    const std::array<std::pair<std::string_view, std::size_t>, 5> known{
        {{"\xEF\xBB\xBFline\r\nnext", 2U},
         {"one\r\ntwo\r\n", 4U},
         {"\xE6\x97\xA5\xE6\x9C\xAC", 1U},
         {"\x93\xFA\x96\x7B\r\n", 1U},
         {std::string_view("\xFF\xFE\x0D\x00\x0A\x00", 6), 3U}}};
    for (const auto &[bytes, chunk] : known)
    {
        write_sample(bytes);
        const Opened bulk = bulk_read(worker, files, code_pages);
        expect(bulk.has_value(), "the known sample opens whole");
        expect(same_opened(bulk, chunked_read(worker, files, code_pages, chunk).first),
               "a known telling cut reads the same as a whole read");
    }
}

[[nodiscard]] std::size_t broken_inputs(adapters::Win32Worker &worker,
                                        adapters::Win32FileAdapter &files,
                                        adapters::Win32CodePageAdapter &code_pages)
{
    Random random{chunk_seed};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < chunk_inputs; ++index)
    {
        const std::string bytes = makers.at(index % makers.size())(random);
        const auto violation =
            input_violation(random, bytes, ChunkPorts{worker, files, code_pages});
        if (!violation.empty())
        {
            ++broken;
            std::fprintf(stderr, "document chunk input %zu (seed %llu): %.*s\n", index,
                         static_cast<unsigned long long>(chunk_seed),
                         static_cast<int>(violation.size()), violation.data());
        }
    }
    return broken;
}

int run_tests()
{
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32CodePageAdapter code_pages;
    DeleteFileW(sample_name);
    verify_known_cuts(worker, files, code_pages);
    expect(broken_inputs(worker, files, code_pages) == 0U,
           "reading in chunks of any size opens what reading whole opens");
    expect(DeleteFileW(sample_name) != 0, "the sample file is removed");
    std::printf("Document chunk property: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"document-chunk");
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
