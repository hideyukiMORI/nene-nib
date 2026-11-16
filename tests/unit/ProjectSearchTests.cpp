// 契約 `--project-search`（ADR 0167 の決定 3・7 の core の分・#1003 の段 G1）。
// 1. 検索語: literal は ASCII の英字だけ大小を区別しない（全角とほかの文字は畳まない）、先頭の `/`
//    は VimPattern（大小は区別する・`\c` で畳む）、空の検索語はどこにも当たらない。
// 2. 行と桁の単位（PlaceCaret へ渡す形）: 行は 1 始まり、桁は code point の 1 始まり。CRLF の CR と
//    先頭の BOM は本文に入れない。正しい UTF-8 でない行は照合しない。1 行に当たりは 1 つ。
// 3. 塊の境の行の繰り越し・1 MiB を超える行を飛ばす・room で打ち切る。
// 4. 256 バイトの窓: 行頭から入れば行頭から、入らなければ当たりの側へ寄せる。code point
// を割らない。
// 5. NUL（最初の 8 KiB）・飛ばすフォルダの表・16 MiB の上限。
// 6. 性質の試験: 固定 seed の乱数の本文を乱数の塊に切って探した当たりが、全文を 1 つの塊で探した
//    当たりと、行に分けて素朴に探した写しと一致する。
#include "ProjectHit.hpp"
#include "ProjectHits.hpp"
#include "ProjectQuery.hpp"
#include "ProjectScan.hpp"
#include "ProjectSearchRules.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimPatternFailure.hpp"

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
           "a literal folds ASCII letters and gives one hit per line at the leftmost match");
    expect(hits.size() == 3 && matched_text(hits.at(0)) == "Foo" && hits.at(0).text_begin == 0,
           "the hit marks its matched bytes in the line text");
    expect(whole_hits("ＦＯＯ\nｆｏｏ\n", query_of("ｆｏｏ")).size() == 1 &&
               whole_hits("Éa\néa\n", query_of("éa")).size() == 1,
           "a literal does not fold full-width or non-ASCII letters");
    expect(whole_hits("a.b\naxb\n", query_of("a.b")).size() == 1 &&
               whole_hits("x/usr/bin\n", query_of("x/usr")).size() == 1,
           "a literal is bytes, not a pattern");
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

using Spots = std::vector<std::pair<std::size_t, std::size_t>>;

[[nodiscard]] std::string ascii_folded(std::string_view text)
{
    std::string folded(text);
    std::ranges::transform(
        folded, folded.begin(),
        [](char byte) { return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte + 32) : byte; });
    return folded;
}

// 素朴な写しの 1 行: BOM（1 行目だけ）と行末の CR を除き、ASCII を畳む。
[[nodiscard]] std::string model_line(std::string_view line, std::size_t number)
{
    if (number == 1 && line.starts_with("\xEF\xBB\xBF"))
    {
        line.remove_prefix(3);
    }
    if (line.ends_with('\r'))
    {
        line.remove_suffix(1);
    }
    return ascii_folded(line);
}

// 素朴な写し: 行に分け、UTF-8 の行だけを ASCII を畳んで最も左を探す（行と桁の組）。
[[nodiscard]] Spots model_hits(std::string_view text, std::string_view needle)
{
    const std::string folded_needle = ascii_folded(needle);
    Spots hits;
    std::size_t number = 1;
    for (std::size_t at = 0; at < text.size(); ++number)
    {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        const std::string line = model_line(text.substr(at, end - at), number);
        at = end + 1;
        const std::size_t found = line.find(folded_needle);
        if (core::validate_utf8(line).has_value() && found != std::string::npos)
        {
            hits.emplace_back(number,
                              core::code_point_count(std::string_view{line}.substr(0, found)) + 1);
        }
    }
    return hits;
}

[[nodiscard]] Spots spots_of(const Hits &hits)
{
    Spots spots;
    for (const auto &hit : hits)
    {
        spots.emplace_back(hit.line.value, hit.column.value);
    }
    return spots;
}

constexpr std::array<std::string_view, 10> pieces{"a",    "A",  "b", "B",  "\n",
                                                  "\r\n", "\r", "é", "日", "\xFF"};
constexpr std::array<std::string_view, 8> literal_needles{"a", "ab",  "AB",   "bA",
                                                          "é", "a日", "Ba\r", "日b"};
constexpr std::array<std::string_view, 5> pattern_needles{"/a\\+b", "/^b", "/a$", "/[bé]",
                                                          "/\\ca.b"};

[[nodiscard]] std::string random_text(Random &random)
{
    std::string text = random() % 4 == 0 ? "\xEF\xBB\xBF" : "";
    const std::size_t length = random() % 400;
    for (std::size_t index = 0; index < length; ++index)
    {
        // \xFF（UTF-8 でない行）は稀にする。
        const std::size_t piece = random() % (pieces.size() * 8);
        text +=
            piece < pieces.size() * 8 - 1 ? pieces.at(piece % (pieces.size() - 1)) : pieces.back();
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

void verify_chunk_property()
{
    Random random(seed);
    bool same = true;
    bool modeled = true;
    for (std::size_t round = 0; round < 3000; ++round)
    {
        const std::string text = random_text(random);
        const bool literal = round % 3 != 0;
        const std::string_view needle = literal
                                            ? literal_needles.at(random() % literal_needles.size())
                                            : pattern_needles.at(random() % pattern_needles.size());
        const auto query = query_of(needle);
        const Hits whole = whole_hits(text, query);
        same = same && chunked_hits(text, query, random_cuts(random, text.size())) == whole;
        modeled = modeled && (!literal || spots_of(whole) == model_hits(text, needle));
    }
    expect(same, "hits from random chunks equal the hits from the whole text");
    expect(modeled, "literal hits equal a naive line-by-line search");
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
}
} // namespace nenenib::tests
