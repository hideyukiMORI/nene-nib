// Issue #1290: 開く道の worker の塊の読み（DocumentChunkRead・ADR 0159 の決定 2・3）を、塊の境が
// どこに来ても一括の読みと同じになるかで見る性質の試験。固定 seed の決定的な生成器で本文（UTF-8・
// UTF-8 BOM・Shift_JIS・UTF-16 LE / BE・BOM の欠片・でたらめなバイト列。改行は CRLF・LF・lone CR を
// 混ぜる）を作って本物のファイルに書き、次の 2 本で読んで比べる。族は正しい本文だけのもの（UTF-8・
// UTF-8 BOM・Shift_JIS・UTF-16 LE / BE。一括でも開けなければならない）と、不正なバイト・途中で
// 切れた多バイト・孤立したサロゲート・奇数長を一部に混ぜたものの半々。成否と境の内訳（開けた・
// 失敗した・UTF-8 で開けた入力、UTF-8 の文字や BOM の途中で切れた読み）の件数に下限を置く。
//   一括: Win32DocumentReader の既定（64 MiB 以下は呼んだスレッドで SynchronousDocumentReader）。
//   塊:   同期の上限を 0 にした Win32DocumentReader（1 バイト以上は worker の塊の仕事）に、塊の
//         大きさ k を渡す。k は 1・2・3 と、最初の境が多バイトの途中・CR と LF の間・BOM の途中に
//         来る値（その位置そのもの）と、でたらめな値。
// 不変条件は「分割しても同じ」: 成否・失敗の種類・本文のバイト列・文字コード・改行の形・行数が
// 一括と同じで、塊の読みは ceil(大きさ / k) 回の進み具合を届けて最後に読んだ量がファイルの大きさ。
// CP932 は本物の Win32CodePageAdapter。置き場は temporary_test_directory（#1103）の中の固定の名前。
// 時刻は読まない（待つのは合図の受け皿のセマフォだけで、止まらなければ CTest の制限時間で落ちる）。
// 生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。回数は Debug で数秒以内の値。
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
constexpr std::size_t chunk_inputs = 96U * rounds;
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

// 欠片は「正しい」ものと「壊れた」ものに分ける。正しい族は正しい欠片だけを並べ、混ぜた族だけが
// 欠片ごとに 1/broken_odds の割合で壊れた欠片を引く（#1301 のレビュー: 16 片中 3 片が壊れていると
// ほとんどの入力が不正になり、正しい本文の上で塊の境が多バイトや BOM
// の途中に来る形がほぼ試されない）。

// UTF-8 の正しい欠片。改行 3 種・2〜4 バイトの文字・結合文字・幅 0 の空白・NEL（C2 85）。
constexpr std::array<std::string_view, 13> utf8_pieces{
    {"a", "word ", "\t", "\n", "\r\n", "\r", "\xC3\xA9", "\xE6\x97\xA5\xE6\x9C\xAC", "\xE3\x81\x8B",
     "\xF0\x9F\x98\x80", "e\xCC\x81", "\xE2\x80\x8B", "\xC2\x85"}};
// UTF-8 の壊れた欠片。続きのバイトだけ・0xFF・途中で切れた 3 バイトの列。
constexpr std::array<std::string_view, 3> utf8_broken{{"\x80", "\xFF", "\xE3\x81"}};

// CP932 の正しい欠片。改行 3 種・2 バイトの文字（2 バイト目が ASCII
// の範囲のものを含む）・半角カナ。
constexpr std::array<std::string_view, 12> cp932_pieces{
    {"a", "text ", "\n", "\r\n", "\r", "\x93\xFA\x96\x7B", "\x82\xA0", "\x83\x5C", "\x81\x40",
     "\xB1\xB2", "\x88\x9F", "\xE0\x40"}};
// CP932 の壊れた欠片。後ろの無い先頭バイト・0x80・0xA0・0xFD。
constexpr std::array<std::string_view, 4> cp932_broken{{"\x81", "\x80", "\xA0", "\xFD"}};

// UTF-16 の正しい単位（ホストの値）。改行 3 種・BMP・サロゲートの対・本文の中の U+FEFF。
constexpr std::array<std::u16string_view, 10> utf16_pieces{{u"a", u"word ", u"\n", u"\r\n", u"\r",
                                                            u"\u65E5\u672C", u"\u3042",
                                                            u"\U0001F600", u"\u00E9", u"\uFEFF"}};
// UTF-16 の壊れた単位。孤立したサロゲート。
constexpr std::array<std::u16string_view, 2> utf16_broken{{u"\xD800", u"\xDC00"}};

constexpr std::size_t broken_odds = 6U;

// 正しい欠片を並べる。dirty なら欠片ごとに 1/broken_odds で壊れた欠片を引く。
template <typename Piece>
[[nodiscard]] std::vector<Piece> picked(Random &random, std::span<const Piece> pieces,
                                        std::span<const Piece> broken, bool dirty)
{
    std::vector<Piece> chosen;
    const std::size_t count = below(random, piece_limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (dirty && below(random, broken_odds) == 0U)
        {
            chosen.push_back(broken[below(random, broken.size())]);
            continue;
        }
        chosen.push_back(pieces[below(random, pieces.size())]);
    }
    return chosen;
}

[[nodiscard]] std::string joined(Random &random, std::span<const std::string_view> pieces,
                                 std::span<const std::string_view> broken, bool dirty)
{
    std::string text;
    for (const std::string_view piece : picked(random, pieces, broken, dirty))
    {
        text.append(piece);
    }
    return text;
}

[[nodiscard]] std::string utf8_text(Random &random)
{
    return joined(random, utf8_pieces, utf8_broken, false);
}

[[nodiscard]] std::string utf8_bom_text(Random &random)
{
    return "\xEF\xBB\xBF" + joined(random, utf8_pieces, utf8_broken, false);
}

[[nodiscard]] std::string utf8_dirty_text(Random &random)
{
    return joined(random, utf8_pieces, utf8_broken, true);
}

[[nodiscard]] std::string utf8_bom_dirty_text(Random &random)
{
    return "\xEF\xBB\xBF" + joined(random, utf8_pieces, utf8_broken, true);
}

[[nodiscard]] std::string cp932_text(Random &random)
{
    return joined(random, cp932_pieces, cp932_broken, false);
}

[[nodiscard]] std::string cp932_dirty_text(Random &random)
{
    return joined(random, cp932_pieces, cp932_broken, true);
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

// dirty なら孤立したサロゲートを混ぜ、ときどき奇数長にする。
[[nodiscard]] std::string utf16_text(Random &random, bool little, bool dirty)
{
    using Units = std::span<const std::u16string_view>;
    std::string bytes = little ? std::string("\xFF\xFE") : std::string("\xFE\xFF");
    for (const std::u16string_view units :
         picked(random, Units(utf16_pieces), Units(utf16_broken), dirty))
    {
        append_units(bytes, units, little);
    }
    if (dirty && below(random, 3U) == 0U)
    {
        bytes.push_back('A');
    }
    return bytes;
}

[[nodiscard]] std::string utf16_le_text(Random &random)
{
    return utf16_text(random, true, false);
}

[[nodiscard]] std::string utf16_be_text(Random &random)
{
    return utf16_text(random, false, false);
}

[[nodiscard]] std::string utf16_le_dirty_text(Random &random)
{
    return utf16_text(random, true, true);
}

[[nodiscard]] std::string utf16_be_dirty_text(Random &random)
{
    return utf16_text(random, false, true);
}

// BOM の欠片（UTF-8 の 2 バイトまで・UTF-16 の 1 バイト・UTF-32 の BOM）に本文が続く。
[[nodiscard]] std::string bom_fragment_text(Random &random)
{
    constexpr std::array<std::string_view, 5> fragments{
        {"\xEF", "\xEF\xBB", "\xFF", "\xFE", std::string_view("\xFF\xFE\x00\x00", 4)}};
    return std::string(fragments.at(below(random, fragments.size()))) +
           joined(random, utf8_pieces, utf8_broken, true);
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

// 族と「正しい本文か」（正しい族の入力は一括でも開けなければならない）。
using Family = std::pair<Make, bool>;
constexpr std::array<Family, 12> makers{{{utf8_text, true},
                                         {utf8_bom_text, true},
                                         {cp932_text, true},
                                         {utf16_le_text, true},
                                         {utf16_be_text, true},
                                         {utf8_dirty_text, false},
                                         {utf8_bom_dirty_text, false},
                                         {cp932_dirty_text, false},
                                         {utf16_le_dirty_text, false},
                                         {utf16_be_dirty_text, false},
                                         {bom_fragment_text, false},
                                         {random_text, false}}};

// 成否と境の内訳（生成器が狙いに届いているかの守り）。添字は下の定数。
constexpr std::size_t opened_inputs = 0U;
constexpr std::size_t failed_inputs = 1U;
constexpr std::size_t utf8_opened_inputs = 2U;
constexpr std::size_t mid_multibyte_reads = 3U;
constexpr std::size_t mid_bom_reads = 4U;
using Tally = std::array<std::size_t, 5>;

Tally &tally()
{
    static Tally counts{};
    return counts;
}

[[nodiscard]] bool opened_as_utf8(const Opened &opened)
{
    return opened.has_value() && (opened.value().encoding == core::TextEncoding::utf8 ||
                                  opened.value().encoding == core::TextEncoding::utf8_bom);
}

// UTF-8 として開けた本文で、塊の境（k の倍数）のどれかが続きのバイトの前に来るか。
[[nodiscard]] bool splits_multibyte(std::string_view bytes, std::size_t chunk)
{
    for (std::size_t at = chunk; at < bytes.size(); at += chunk)
    {
        const auto value = static_cast<unsigned char>(bytes[at]);
        if (value >= 0x80U && value <= 0xBFU)
        {
            return true;
        }
    }
    return false;
}

void count_whole(const Opened &bulk)
{
    ++tally()[bulk.has_value() ? opened_inputs : failed_inputs];
    if (opened_as_utf8(bulk))
    {
        ++tally()[utf8_opened_inputs];
    }
}

void count_cut(const Opened &bulk, std::string_view bytes, std::size_t chunk)
{
    if (!opened_as_utf8(bulk))
    {
        return;
    }
    if (splits_multibyte(bytes, chunk))
    {
        ++tally()[mid_multibyte_reads];
    }
    if (bulk.value().encoding == core::TextEncoding::utf8_bom && chunk < 3U && bytes.size() > chunk)
    {
        ++tally()[mid_bom_reads];
    }
}

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
// 正しい族の本文（clean）は一括でも開けなければならない。
[[nodiscard]] std::string_view input_violation(Random &random, std::string_view bytes, bool clean,
                                               const ChunkPorts &ports)
{
    auto &[worker, files, code_pages] = ports;
    write_sample(bytes);
    const Opened bulk = bulk_read(worker, files, code_pages);
    count_whole(bulk);
    if (clean && !bulk.has_value())
    {
        return "an input of a clean family opens whole";
    }
    for (const std::size_t chunk : chunk_sizes(random, bytes))
    {
        count_cut(bulk, bytes, chunk);
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
        const auto &[make, clean] = makers.at(index % makers.size());
        const std::string bytes = make(random);
        const auto violation =
            input_violation(random, bytes, clean, ChunkPorts{worker, files, code_pages});
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

// 生成器が狙いに届いているかの守り（#1301 のレビュー）: 開けた入力・失敗した入力・UTF-8 として
// 開けた入力・UTF-8 の多バイトの途中に境が来た読み・BOM の途中に境が来た読みが、どれも下限以上。
// 下限は seed 12901 の実測（43・53・16・99・27）のおよそ半分。
void verify_reach()
{
    const Tally &counts = tally();
    std::printf("Document chunk reach: %zu opened, %zu failed, %zu opened as UTF-8, "
                "%zu reads cut a UTF-8 character, %zu reads cut a BOM\n",
                counts[opened_inputs], counts[failed_inputs], counts[utf8_opened_inputs],
                counts[mid_multibyte_reads], counts[mid_bom_reads]);
    expect(counts[opened_inputs] >= 20U, "enough inputs open whole");
    expect(counts[failed_inputs] >= 25U, "enough inputs fail whole");
    expect(counts[utf8_opened_inputs] >= 8U, "enough inputs open as UTF-8");
    expect(counts[mid_multibyte_reads] >= 50U, "enough reads cut a UTF-8 character in the middle");
    expect(counts[mid_bom_reads] >= 13U, "enough reads cut a UTF-8 BOM in the middle");
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
    verify_reach();
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
