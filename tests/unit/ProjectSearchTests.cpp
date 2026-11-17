// 契約 `--project-search`（ADR 0167 の決定 3・7 の core の分・#1003 の段 G1）。
// 1. 検索語: literal は Ctrl+F と同じ逃がしの VimPattern で大小を畳む（全角・Kelvin の K・ſ も
//    vim_folded のとおり）、先頭の `/` は VimPattern（大小は区別する・`\c` で畳む）、空の検索語は
//    どこにも当たらない。
// 2. 行と桁の単位（PlaceCaret へ渡す形）: 行は 1 始まり、桁は code point の 1 始まり。CRLF の CR と
//    先頭の BOM は本文に入れない。正しい UTF-8 でない行は照合しない。1 行に当たりは 1 つ。
// 3. 塊の境の行の繰り越し・1 MiB を超える行を飛ばす・room で打ち切る。
// 4. 256 バイトの窓: 行頭から入れば行頭から、入らなければ当たりの側へ寄せる。code point
// を割らない。
// 5. NUL（最初の 8 KiB）・飛ばすフォルダの表・16 MiB の上限。
// 6. 性質の試験: 固定 seed の乱数の本文を乱数の塊に切って探した当たりが、全文を 1 つの塊で探した
//    当たりと、絞り込み無しで全部の行に照合器を当てた写しと一致する。乱数の room で塊をまたいで
//    打ち切っても先頭の当たりのまま。VimPattern::required_bytes は当たる行に必ず現れる。
#include "AsciiCase.hpp"
#include "LiteralPattern.hpp"
#include "ProjectHit.hpp"
#include "ProjectHits.hpp"
#include "ProjectQuery.hpp"
#include "ProjectScan.hpp"
#include "ProjectSearchRules.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimCase.hpp"
#include "VimPattern.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternNeedle.hpp"
#include "VimSearchDirection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;

using Random = std::mt19937_64;
using Hits = std::vector<core::ProjectHit>;

constexpr std::uint64_t seed = 1003;
constexpr std::size_t unlimited = core::project_search_hit_limit;

[[nodiscard]] core::ProjectQuery query_of(std::string_view input)
{
    return core::ProjectQuery::parse(input).value();
}

// 最後の塊 chunk の後にファイルの終わりを渡す。
[[nodiscard]] Hits finished_hits(const core::ProjectScan &scan, std::string_view chunk,
                                 const core::ProjectQuery &query, std::size_t room)
{
    auto answer = core::project_hits_in(scan, chunk, query, room);
    const auto rest = core::project_hits_finished(answer.next, query, room - answer.hits.size());
    answer.hits.insert(answer.hits.end(), rest.hits.begin(), rest.hits.end());
    return answer.hits;
}

// 全文を 1 つの塊で探す。
[[nodiscard]] Hits whole_hits(std::string_view text, const core::ProjectQuery &query,
                              std::size_t room = unlimited)
{
    return finished_hits(core::ProjectScan{}, text, query, room);
}

// cuts（昇順のバイト位置）で切った塊を順に渡す。最後の塊は空でもよい。
[[nodiscard]] Hits chunked_hits(std::string_view text, const core::ProjectQuery &query,
                                const std::vector<std::size_t> &cuts)
{
    Hits hits;
    core::ProjectScan scan;
    std::size_t at = 0;
    for (const std::size_t cut : cuts)
    {
        auto answer = core::project_hits_in(scan, text.substr(at, cut - at), query, unlimited);
        hits.insert(hits.end(), answer.hits.begin(), answer.hits.end());
        scan = std::move(answer.next);
        at = cut;
    }
    const Hits last = finished_hits(scan, text.substr(at), query, unlimited - hits.size());
    hits.insert(hits.end(), last.begin(), last.end());
    return hits;
}

[[nodiscard]] bool hit_is(const core::ProjectHit &hit, std::size_t line, std::size_t column,
                          std::string_view text)
{
    return hit.line.value == line && hit.column.value == column && hit.line_text == text;
}

[[nodiscard]] std::string_view matched_text(const core::ProjectHit &hit)
{
    return std::string_view{hit.line_text}.substr(hit.match_begin, hit.match_end - hit.match_begin);
}

void verify_literal_queries()
{
    const std::string_view text = "alpha Foo\nFOO bar\nnothing\nfOo foo\n";
    const Hits hits = whole_hits(text, query_of("foo"));
    expect(hits.size() == 3 && hit_is(hits.at(0), 1, 7, "alpha Foo") &&
               hit_is(hits.at(1), 2, 1, "FOO bar") && hit_is(hits.at(2), 4, 1, "fOo foo"),
           "a literal folds case and gives one hit per line at the leftmost match");
    expect(hits.size() == 3 && matched_text(hits.at(0)) == "Foo" && hits.at(0).text_begin == 0,
           "the hit marks its matched bytes in the line text");
    expect(
        whole_hits("ＦＯＯ\nｆｏｏ\n", query_of("ｆｏｏ")).size() == 2 &&
            whole_hits("Éa\néa\n", query_of("éa")).size() == 2 &&
            whole_hits("\xE2\x84\xAA"
                       "ey\nkey\n",
                       query_of("KEY"))
                    .size() == 2 &&
            whole_hits("\xC5\xBF"
                       "et\nset\n",
                       query_of("SET"))
                    .size() == 2,
        "a literal folds case like the Vim pattern with ignorecase (full width, Kelvin, long s)");
    expect(whole_hits("e\xCC\x81"
                      "x\nex\n",
                      query_of("ex"))
                       .size() == 1 &&
               whole_hits("e\xCC\x81"
                          "x\nex\n",
                          query_of("ex"))
                       .at(0)
                       .line.value == 2,
           "a literal does not hit in the middle of a character with a combining mark");
    expect(whole_hits("a.b\naxb\n", query_of("a.b")).size() == 1 &&
               whole_hits("x/usr/bin\n", query_of("x/usr")).size() == 1 &&
               whole_hits("a\\b\n", query_of("a\\b")).size() == 1,
           "a literal is escaped like Ctrl+F, not read as a pattern");
    expect(whole_hits("12 + 3\n", query_of("+ 3")).size() == 1 &&
               whole_hits("日本語の文\n", query_of("語の")).size() == 1,
           "a literal without letters is matched as plain bytes");
    expect(query_of("").empty() && whole_hits("a\n\nb\n", query_of("")).empty() &&
               query_of("/").empty() && whole_hits("a\n", query_of("/")).empty(),
           "an empty literal and a bare slash search nothing");
    expect(!core::ProjectQuery::parse("\xE3\x81").has_value() &&
               core::ProjectQuery::parse("\xE3\x81").error() ==
                   core::VimPatternFailure::invalid_utf8,
           "a literal that is not UTF-8 is refused");
}

void verify_pattern_queries()
{
    const std::string_view text = "foo1\nFoo22\nbar\nfoooo\n";
    const Hits plain = whole_hits(text, query_of("/fo\\+"));
    expect(plain.size() == 2 && hit_is(plain.at(0), 1, 1, "foo1") &&
               hit_is(plain.at(1), 4, 1, "foooo") && matched_text(plain.at(1)) == "foooo",
           "a slash makes a Vim pattern that respects case like Vim's default");
    expect(whole_hits(text, query_of("/\\cfoo")).size() == 3, "the pattern folds case with \\c");
    expect(whole_hits(text, query_of("/[0-9]\\+$")).size() == 2 &&
               matched_text(whole_hits(text, query_of("/[0-9]\\+$")).at(1)) == "22",
           "a pattern that has no literal bytes checks every line");
    const Hits starts = whole_hits("a\n\nb\n", query_of("/^"));
    expect(starts.size() == 3 && starts.at(1).line.value == 2 &&
               starts.at(1).match_begin == starts.at(1).match_end,
           "a zero-width pattern hits every line, empty lines too");
    expect(!core::ProjectQuery::parse("/a\\(").has_value() &&
               core::ProjectQuery::parse("/a\\(").error() == core::VimPatternFailure::group &&
               !core::ProjectQuery::parse("/a/b").has_value(),
           "a pattern that VimPattern refuses is refused with its failure");
    expect(whole_hits("/usr/bin\n", query_of("/\\/usr")).size() == 1,
           "a slash inside the pattern is written as in Vim's search");
}

void verify_units()
{
    const Hits wide = whole_hits("日本語foo\r\nxx\r\n\tfoo\r\n", query_of("foo"));
    expect(wide.size() == 2 && hit_is(wide.at(0), 1, 4, "日本語foo") &&
               hit_is(wide.at(1), 3, 2, "\tfoo"),
           "the column counts code points from 1 and the CR is not part of the line");
    const Hits marked = whole_hits("\xEF\xBB\xBF"
                                   "abc foo\nfoo\n",
                                   query_of("foo"));
    expect(marked.size() == 2 && hit_is(marked.at(0), 1, 5, "abc foo") &&
               hit_is(marked.at(1), 2, 1, "foo"),
           "the UTF-8 byte order mark of the file is not part of the first line");
    expect(whole_hits("a\xEF\xBB\xBF"
                      "foo\n",
                      query_of("foo"))
                   .at(0)
                   .column.value == 3,
           "a byte order mark inside the text is an ordinary character");
    const Hits broken = whole_hits("foo \x82\xA0\nfoo\n", query_of("foo"));
    expect(broken.size() == 1 && hit_is(broken.at(0), 2, 1, "foo"),
           "a line that is not UTF-8 is not searched and the lines after it keep their numbers");
    const Hits lone = whole_hits("a\rfoo\nfoo", query_of("foo"));
    expect(lone.size() == 2 && hit_is(lone.at(0), 1, 3, "a\rfoo") &&
               hit_is(lone.at(1), 2, 1, "foo"),
           "a lone CR stays in its line and the last line without a newline is searched");
}

void verify_chunk_boundaries()
{
    const std::string text = "first foo\nsplit 日本 foo here\nlast foo";
    // 行の途中・code point の途中・改行の直前・直後で切る。
    for (const std::size_t cut : {std::size_t{3}, std::size_t{9}, std::size_t{10}, std::size_t{17},
                                  std::size_t{18}, std::size_t{30}})
    {
        expect(chunked_hits(text, query_of("foo"), {cut}) == whole_hits(text, query_of("foo")),
               "a line cut at a chunk boundary is carried to the next chunk");
    }
    expect(chunked_hits(text, query_of("foo"), {2, 4, 6, 8, 12, 16, 20}) ==
               whole_hits(text, query_of("foo")),
           "a line carried over several chunks is still one line");
    const auto first =
        core::project_hits_in(core::ProjectScan{}, "abc\nde", query_of("x"), unlimited);
    expect(first.next.line.value == 2 && first.next.carry == "de" && !first.next.skipping,
           "the scan carries at most the one cut line and counts the lines before it");
}

void verify_long_lines()
{
    const std::size_t limit = core::project_search_line_limit;
    std::string text(limit - 3, 'a');
    text += "foo\n";
    text += std::string(limit - 2, 'b') + "foo\nfoo\n";
    const Hits hits = whole_hits(text, query_of("foo"));
    expect(hits.size() == 2 && hits.at(0).line.value == 1 && hits.at(1).line.value == 3,
           "a line of the limit is searched and a longer line is skipped");
    expect(hits.size() == 2 && hits.at(0).column.value == limit - 2,
           "the column of a hit in a long line counts from the line start");
    for (const std::vector<std::size_t> &cuts :
         {std::vector<std::size_t>{limit / 2, limit + 10, limit + limit / 2},
          std::vector<std::size_t>{10, limit - 1, limit + 1, 2 * limit + 1},
          std::vector<std::size_t>{limit + 4, limit + 5}})
    {
        expect(chunked_hits(text, query_of("foo"), cuts) == hits,
               "a skipped long line is skipped wherever the chunks are cut");
    }
    std::string tail(limit + 1, 'c');
    tail.insert(0, "foo\n");
    tail += "foo";
    expect(whole_hits(tail, query_of("foo")).size() == 1 &&
               chunked_hits(tail, query_of("foo"), {6, limit}).size() == 1,
           "a last line longer than the limit without a newline is skipped");
}

void verify_windows()
{
    const Hits head = whole_hits(std::string(300, 'x') + "\n", query_of("x"));
    expect(head.size() == 1 && head.at(0).line_text.size() == core::project_hit_text_bytes &&
               head.at(0).text_begin == 0,
           "a hit near the line start shows the first 256 bytes");
    const Hits far = whole_hits(std::string(1000, '-') + "needle" + std::string(500, '-') + "\n",
                                query_of("needle"));
    expect(far.size() == 1 && far.at(0).column.value == 1001 &&
               far.at(0).text_begin == 1000 - core::project_hit_context_bytes &&
               matched_text(far.at(0)) == "needle" &&
               far.at(0).match_begin == core::project_hit_context_bytes &&
               far.at(0).line_text.size() == core::project_hit_text_bytes,
           "a hit beyond the first 256 bytes moves the window to the hit");
    std::string kana;
    for (std::size_t index = 0; index < 120; ++index)
    {
        kana += "あ";
    }
    const Hits wide = whole_hits(kana + "needle" + kana + "\n", query_of("needle"));
    const auto &shown = wide.at(0);
    expect(wide.size() == 1 && shown.column.value == 121 &&
               core::validate_utf8(shown.line_text).has_value() &&
               shown.line_text.size() <= core::project_hit_text_bytes &&
               matched_text(shown) == "needle" && shown.text_begin % 3 == 0,
           "the moved window starts and ends on code point boundaries");
    const Hits cut = whole_hits(kana + "\n", query_of("あ"));
    expect(cut.size() == 1 && cut.at(0).line_text.size() == 255 &&
               core::validate_utf8(cut.at(0).line_text).has_value(),
           "the window from the line start does not split the last code point");
    const Hits longer = whole_hits(std::string(200, '-') + std::string(400, 'n') + "\n",
                                   query_of(std::string(400, 'n')));
    expect(longer.size() == 1 && longer.at(0).match_end == longer.at(0).line_text.size() &&
               longer.at(0).match_begin == core::project_hit_context_bytes,
           "a hit longer than the window is cut at the window end");
}

void verify_rules()
{
    std::string late(core::project_search_binary_probe, 'a');
    late += '\0';
    expect(core::project_text_is_binary(std::string_view{"ab\0cd", 5}) &&
               !core::project_text_is_binary("plain text") && !core::project_text_is_binary(late),
           "a NUL in the first 8 KiB marks a binary file");
    expect(core::project_folder_skipped(".git") && core::project_folder_skipped("node_modules") &&
               core::project_folder_skipped("Build") && core::project_folder_skipped("OUT") &&
               !core::project_folder_skipped("builds") && !core::project_folder_skipped("src") &&
               !core::project_folder_skipped("git"),
           "the fixed folders are skipped by name without ASCII case");
    expect(core::project_file_fits(core::project_search_file_limit) &&
               !core::project_file_fits(core::project_search_file_limit + 1),
           "a file over 16 MiB is not searched");
    std::string many;
    for (std::size_t index = 0; index < 10; ++index)
    {
        many += "foo\n";
    }
    const auto room = core::project_hits_in(core::ProjectScan{}, many, query_of("foo"), 3);
    expect(room.hits.size() == 3 && room.next.line.value == 11,
           "the room stops the hits and the scan still counts every line");
}

using Spots = std::vector<std::array<std::size_t, 4>>;

// 素朴な写しの 1 行の本文: BOM（1 行目だけ）と行末の CR を除く。
[[nodiscard]] std::string_view model_line(std::string_view line, std::size_t number)
{
    if (number == 1 && line.starts_with("\xEF\xBB\xBF"))
    {
        line.remove_prefix(3);
    }
    if (line.ends_with('\r'))
    {
        line.remove_suffix(1);
    }
    return line;
}

// 絞り込みの無い写し: 行に分け、UTF-8 の行ごとに同じ照合器の matched を呼ぶ（行・桁・当たりの
// 始まりと終わり（行頭からのバイト））。room 件で止める。
[[nodiscard]] Spots model_hits(std::string_view text, const core::VimPattern &pattern,
                               std::size_t room)
{
    Spots hits;
    std::size_t number = 1;
    for (std::size_t at = 0; at < text.size() && hits.size() < room; ++number)
    {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const std::string_view line = model_line(text.substr(at, end - at), number);
        at = end + 1;
        const auto match =
            core::validate_utf8(line).has_value() ? pattern.matched(line, 0) : std::nullopt;
        if (match.has_value())
        {
            hits.push_back({number, core::code_point_count(line.substr(0, match.value().begin)) + 1,
                            match.value().begin, match.value().end});
        }
    }
    return hits;
}

[[nodiscard]] Spots spots_of(const Hits &hits)
{
    Spots spots;
    for (const auto &hit : hits)
    {
        spots.push_back({hit.line.value, hit.column.value, hit.text_begin + hit.match_begin,
                         hit.text_begin + hit.match_end});
    }
    return spots;
}

// 製品と同じ組み立ての照合器（literal は Ctrl+F の逃がし ＋ 大小を畳む・先頭 `/` は区別する）。
[[nodiscard]] core::VimPattern pattern_of(std::string_view needle)
{
    return needle.starts_with('/')
               ? core::VimPattern::parse(needle.substr(1), core::VimSearchDirection::forward,
                                         core::VimPatternCase::sensitive)
                     .value()
               : core::VimPattern::parse(
                     core::literal_pattern(needle, core::VimSearchDirection::forward),
                     core::VimSearchDirection::forward, core::VimPatternCase::ignore)
                     .value();
}

// 本文の部品。k s とその大文字・Kelvin の K（U+212A）・ſ（U+017F）・全角の英字・結合文字
// （U+0301）・4 バイトの字・NUL・長い行（窓が当たりの側へ寄る）・UTF-8 でないバイト。
constexpr std::array<std::string_view, 24> pieces{
    "a",
    "A",
    "b",
    "B",
    "k",
    "K",
    "s",
    "S",
    "e",
    "\n",
    "\r\n",
    "\r",
    "\xC3\xA9",
    "\xC3\x89",
    "日",
    "\x00",
    "\xE2\x84\xAA",
    "\xC5\xBF",
    "\xEF\xBD\x86",
    "\xEF\xBC\xA6",
    "\xCC\x81",
    "\xF0\x9F\x98\x80",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxx",
    "\xFF"};
constexpr std::array<std::string_view, 16> needles{
    "a",         "ab",           "AB",  "k",      "Ks",  "s",   "SK",    "\xC3\xA9",
    "e\xCC\x81", "\xEF\xBD\x86", "a日", "/a\\+b", "/^b", "/k$", "/[bé]", "/\\cs.k"};

[[nodiscard]] std::string_view piece_of(Random &random)
{
    // 長い行と UTF-8 でないバイトは稀にする。
    const std::size_t piece = random() % (pieces.size() * 4);
    return piece < pieces.size() * 4 - 2 ? pieces.at(piece % (pieces.size() - 2))
                                         : pieces.at(pieces.size() - 2 + piece % 2);
}

[[nodiscard]] std::string random_text(Random &random)
{
    std::string text = random() % 4 == 0 ? "\xEF\xBB\xBF" : "";
    const std::size_t length = random() % 400;
    for (std::size_t index = 0; index < length; ++index)
    {
        const std::string_view piece = piece_of(random);
        // 部品の表の "\x00" は長さ 0 の文字列リテラルになるので NUL は 1 バイトで足す。
        text += piece.empty() ? std::string_view{"\0", 1} : piece;
    }
    return text;
}

[[nodiscard]] std::vector<std::size_t> random_cuts(Random &random, std::size_t size)
{
    std::vector<std::size_t> cuts;
    for (std::size_t at = 0; size > 0;)
    {
        at += 1 + random() % 40;
        if (at >= size)
        {
            break;
        }
        cuts.push_back(at);
    }
    return cuts;
}

// cuts で切った塊を順に、残りの room を渡して照合する（room が塊をまたいで尽きる場合も見る）。
[[nodiscard]] Hits roomed_hits(std::string_view text, const core::ProjectQuery &query,
                               const std::vector<std::size_t> &cuts, std::size_t room)
{
    Hits hits;
    core::ProjectScan scan;
    std::size_t at = 0;
    for (const std::size_t cut : cuts)
    {
        auto answer =
            core::project_hits_in(scan, text.substr(at, cut - at), query, room - hits.size());
        hits.insert(hits.end(), answer.hits.begin(), answer.hits.end());
        scan = std::move(answer.next);
        at = cut;
    }
    const Hits last = finished_hits(scan, text.substr(at), query, room - hits.size());
    hits.insert(hits.end(), last.begin(), last.end());
    return hits;
}

// 絞り込みのバイト列は当たる行に必ず現れる（VimPattern::required_bytes・#1003）。
[[nodiscard]] bool needle_in(std::string_view line, const core::VimPatternNeedle &needle)
{
    const auto folded = [](std::string_view text)
    {
        std::string out(text);
        std::ranges::transform(out, out.begin(), [](char byte)
                               { return static_cast<char>(core::ascii_folded(byte)); });
        return out;
    };
    return needle.ascii_folded ? folded(line).find(needle.bytes) != std::string::npos
                               : line.find(needle.bytes) != std::string_view::npos;
}

[[nodiscard]] bool needle_holds(std::string_view text, const core::VimPattern &pattern)
{
    const auto needle = pattern.required_bytes();
    if (!needle.has_value())
    {
        return true;
    }
    bool held = true;
    std::size_t number = 1;
    for (std::size_t at = 0; at < text.size(); ++number)
    {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const std::string_view line = model_line(text.substr(at, end - at), number);
        at = end + 1;
        held = held && (!core::validate_utf8(line).has_value() ||
                        !pattern.matched(line, 0).has_value() || needle_in(line, needle.value()));
    }
    return held;
}

void verify_chunk_property()
{
    Random random(seed);
    bool same = true;
    bool modeled = true;
    bool roomed = true;
    bool held = true;
    for (std::size_t round = 0; round < 3000; ++round)
    {
        const std::string text = random_text(random);
        const std::string_view needle = needles.at(random() % needles.size());
        const auto query = query_of(needle);
        const auto pattern = pattern_of(needle);
        const Hits whole = whole_hits(text, query);
        same = same && chunked_hits(text, query, random_cuts(random, text.size())) == whole;
        modeled = modeled && spots_of(whole) == model_hits(text, pattern, unlimited);
        const std::size_t room = 1 + random() % 6;
        const Hits limited = roomed_hits(text, query, random_cuts(random, text.size()), room);
        roomed = roomed && limited.size() <= room &&
                 std::equal(limited.begin(), limited.end(), whole.begin(),
                            whole.begin() + static_cast<std::ptrdiff_t>(
                                                std::min(whole.size(), limited.size()))) &&
                 limited.size() == std::min(room, whole.size());
        held = held && needle_holds(text, pattern);
    }
    expect(same, "hits from random chunks equal the hits from the whole text");
    expect(modeled, "the narrowed search finds the same hits as the matcher on every line");
    expect(roomed, "the room cut across chunks keeps the first hits in order");
    expect(held, "the required bytes of a pattern appear on every line it hits");
}

using Folds = std::vector<std::pair<char32_t, char32_t>>;

// 畳んだ形が同じ字の群 [first, last) が、ASCII だけを畳む比較で見分けられない仲間だけか。
[[nodiscard]] bool group_comparable(const Folds &folds, std::size_t first, std::size_t last)
{
    const auto inside = std::count_if(folds.begin() + static_cast<std::ptrdiff_t>(first),
                                      folds.begin() + static_cast<std::ptrdiff_t>(last),
                                      [](const auto &fold) { return fold.second < 0x80; });
    const auto size = static_cast<std::ptrdiff_t>(last - first);
    return inside > 0 ? inside == size : size == 1;
}

// 生成物の case_fold_shared が vim_folded の畳みの仲間と合うか（U+0000〜U+1FFFF の総当たり。写像の
// 表の鍵と行き先はこの範囲に収まり、外の字は自分だけに畳まれる）。
[[nodiscard]] bool comparable_matches_folds()
{
    Folds folds;
    for (char32_t value = 0; value < 0x20000; ++value)
    {
        folds.emplace_back(core::vim_folded(value), value);
    }
    std::ranges::sort(folds);
    bool same = true;
    for (std::size_t first = 0; first < folds.size();)
    {
        const auto end =
            std::find_if(folds.begin() + static_cast<std::ptrdiff_t>(first), folds.end(),
                         [&](const auto &fold) { return fold.first != folds.at(first).first; });
        const auto last = static_cast<std::size_t>(end - folds.begin());
        const bool comparable = group_comparable(folds, first, last);
        same = same &&
               std::all_of(
                   folds.begin() + static_cast<std::ptrdiff_t>(first), end, [&](const auto &fold)
                   { return core::vim_folded_ascii_comparable(fold.second) == comparable; });
        first = last;
    }
    return same;
}

// required_bytes の答えを文字列で（値なしは "-"・ASCII を畳むなら先頭に "~"）。
[[nodiscard]] std::string needle_text(std::string_view source, core::VimPatternCase letter_case)
{
    const auto parsed =
        core::VimPattern::parse(source, core::VimSearchDirection::forward, letter_case);
    if (!parsed.has_value())
    {
        return "!";
    }
    const auto needle = parsed.value().required_bytes();
    if (!needle.has_value())
    {
        return "-";
    }
    return (needle.value().ascii_folded ? "~" : "") + needle.value().bytes;
}

void verify_required_bytes()
{
    expect(comparable_matches_folds(),
           "the generated shared folds agree with vim_folded on every code point");
    const auto sensitive = core::VimPatternCase::sensitive;
    const auto ignore = core::VimPatternCase::ignore;
    expect(needle_text("ne\\+dle", sensitive) == "dle" &&
               needle_text("^\\s*return", sensitive) == "return",
           "the required bytes are the longest run of literals without a count");
    expect(needle_text("NEEDLE", ignore) == "~needle" && needle_text("a日本", ignore) == "~a日本",
           "a folded pattern gives ASCII folded bytes and keeps caseless characters");
    expect(needle_text("ask", ignore) == "~a" && needle_text("\xC3\xA9t\xC3\xA9", ignore) == "~t" &&
               needle_text("k", ignore) == "-" && needle_text("k", sensitive) == "k",
           "a folded k or s or a cased letter outside ASCII cuts the run");
    expect(needle_text("a\\|b", sensitive) == "-" && needle_text(".*", sensitive) == "-",
           "a program or a pattern without literals has no required bytes");
    expect(!core::vim_folded_ascii_comparable(U'k') && !core::vim_folded_ascii_comparable(U'S') &&
               core::vim_folded_ascii_comparable(U'a') && core::vim_folded_ascii_comparable(U'1') &&
               core::vim_folded_ascii_comparable(U'日') &&
               !core::vim_folded_ascii_comparable(U'é') &&
               !core::vim_folded_ascii_comparable(U'\x212A'),
           "only k and s in ASCII have folds from outside ASCII");
}
} // namespace

void verify_project_search_contracts()
{
    verify_literal_queries();
    verify_pattern_queries();
    verify_units();
    verify_chunk_boundaries();
    verify_long_lines();
    verify_windows();
    verify_rules();
    verify_chunk_property();
    verify_required_bytes();
}
} // namespace nenenib::tests
