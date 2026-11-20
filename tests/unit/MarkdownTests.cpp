// parse_markdown（ADR 0151 の決定 5〜8・#765）。入力は CommonMark 0.31 の例を 8 つの構文
// （見出し・段落・強調・リンク・リスト・引用・コード・水平線）と実体参照・改行の範囲で写したもの。
// 期待は CommonMark の HTML を平らな leaf の列に読み替えた値。source_begin は最初の文字の位置。
#include "CaretMotion.hpp"
#include "DisplayLanguage.hpp"
#include "FilePath.hpp"
#include "HeadingSlug.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "Lexer.hpp"
#include "LinkDestination.hpp"
#include "LinkKind.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "MarkdownTask.hpp"
#include "Offset.hpp"
#include "PreviewAlert.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewCodeColor.hpp"
#include "PreviewCodeColors.hpp"
#include "PreviewCodePiece.hpp"
#include "PreviewLink.hpp"
#include "PreviewPoint.hpp"
#include "PreviewSelection.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewSpan.hpp"
#include "PreviewStyle.hpp"
#include "PreviewTableAlign.hpp"
#include "PreviewTableRow.hpp"
#include "Scopes.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::MarkdownFailure;
using core::MarkdownStop;
using core::Offset;
using core::PreviewBlock;
using core::PreviewBlockKind;
using core::PreviewSpan;
using core::PreviewStyle;

constexpr PreviewBlockKind paragraph = PreviewBlockKind::paragraph;
constexpr PreviewBlockKind heading = PreviewBlockKind::heading;
constexpr PreviewBlockKind code = PreviewBlockKind::code;
constexpr PreviewBlockKind html = PreviewBlockKind::html;
constexpr PreviewBlockKind rule = PreviewBlockKind::rule;
constexpr PreviewBlockKind list_item = PreviewBlockKind::list_item;
constexpr PreviewStyle emphasis = PreviewStyle::emphasis;
constexpr PreviewStyle strong = PreviewStyle::strong;
constexpr PreviewStyle inline_code = PreviewStyle::code;
constexpr PreviewStyle link = PreviewStyle::link;
constexpr PreviewStyle image = PreviewStyle::image;
constexpr std::string_view bullet = "•";

// 述語は借りるだけなので、一時の値は受けない（#782 のレビュー R2）。
constexpr auto never_stop = [] { return false; };
template <typename Ask>
concept LendsStop = requires(Ask &&ask) { MarkdownStop::asking(std::forward<Ask>(ask)); };
static_assert(LendsStop<const decltype(never_stop) &>);
static_assert(!LendsStop<decltype(never_stop)>);

[[nodiscard]] PreviewBlock leaf(PreviewBlockKind kind, std::string text, std::size_t begin)
{
    return PreviewBlock{.kind = kind,
                        .level = 0U,
                        .quote_depth = 0U,
                        .list_depth = 0U,
                        .marker = {},
                        .source_begin = Offset{begin},
                        .text = std::move(text),
                        .spans = {},
                        .task = std::nullopt,
                        .table = std::nullopt,
                        .alert = std::nullopt,
                        .code_colors = std::nullopt,
                        .links = {},
                        .piece = core::PreviewCodePiece::only};
}

[[nodiscard]] PreviewBlock titled(std::uint8_t level, std::string text, std::size_t begin)
{
    auto block = leaf(heading, std::move(text), begin);
    block.level = level;
    return block;
}

[[nodiscard]] PreviewBlock styled(PreviewBlock block, std::vector<PreviewSpan> spans)
{
    block.spans = std::move(spans);
    return block;
}

// リンクの宛先の列（#1869）。各リンクは text の中の範囲と宛先。
[[nodiscard]] PreviewBlock linked(PreviewBlock block, std::vector<core::PreviewLink> links)
{
    block.links = std::move(links);
    return block;
}

[[nodiscard]] core::PreviewLink to(std::size_t begin, std::size_t end, std::string target)
{
    return core::PreviewLink{Offset{begin}, Offset{end}, std::move(target)};
}

[[nodiscard]] PreviewBlock listed(PreviewBlock block, std::uint8_t depth, std::string_view marker)
{
    block.list_depth = depth;
    block.marker = std::string(marker);
    return block;
}

// タスクリストの項目（ADR 0155 の追補 22）。mark は `[` の次の 1 バイトの位置。
[[nodiscard]] PreviewBlock tasked(PreviewBlock block, bool checked, std::size_t mark)
{
    block.task = core::MarkdownTask{.checked = checked, .mark_offset = mark};
    return block;
}

[[nodiscard]] PreviewBlock quoted(PreviewBlock block, std::uint8_t depth)
{
    block.quote_depth = depth;
    return block;
}

// 表の行（#1848）。cells は升の始まりと列の揃え。行 0 が見出しの行。
[[nodiscard]] PreviewBlock rowed(std::string text, std::size_t begin, std::uint32_t row,
                                 std::vector<std::pair<std::size_t, core::PreviewTableAlign>> cells)
{
    auto block = leaf(PreviewBlockKind::table_row, std::move(text), begin);
    core::PreviewTableRow table{
        .row = row, .header = row == 0U, .cells = {}, .aligns = {}, .digest = 0U};
    for (const auto &[cell, align] : cells)
    {
        table.cells.push_back(Offset{cell});
        table.aligns.push_back(align);
    }
    block.table = std::move(table);
    return block;
}

// 文書の中の table 番目の表の行（#1883・表の通し番号）。
[[nodiscard]] PreviewBlock numbered(PreviewBlock block, std::uint32_t table)
{
    if (block.table.has_value())
    {
        block.table.value().table = table;
    }
    return block;
}

[[nodiscard]] PreviewBlock alerted(PreviewBlock block, core::PreviewAlert alert)
{
    block.alert = alert;
    return block;
}

[[nodiscard]] PreviewSpan span(std::size_t begin, std::size_t end, PreviewStyle style)
{
    return PreviewSpan{Offset{begin}, Offset{end}, style};
}

void dump_table(const PreviewBlock &block)
{
    if (block.table.has_value())
    {
        const auto &table = block.table.value();
        std::fprintf(stderr, " row(%u,%d)", table.row, table.header ? 1 : 0);
        for (std::size_t at = 0; at < table.cells.size() && at < table.aligns.size(); ++at)
        {
            std::fprintf(stderr, " cell(%zu,%d)", table.cells.at(at).value,
                         static_cast<int>(table.aligns.at(at)));
        }
    }
    if (block.alert.has_value())
    {
        std::fprintf(stderr, " alert(%d)", static_cast<int>(block.alert.value()));
    }
    for (const auto &listed_link : block.links)
    {
        std::fprintf(stderr, " link(%zu,%zu,[%s])", listed_link.begin.value, listed_link.end.value,
                     listed_link.target.c_str());
    }
}

void dump(const std::vector<PreviewBlock> &blocks)
{
    for (const auto &block : blocks)
    {
        std::fprintf(stderr, "  kind=%d level=%d quote=%d list=%d marker=[%s] begin=%zu text=[%s]",
                     static_cast<int>(block.kind), block.level, block.quote_depth, block.list_depth,
                     block.marker.c_str(), block.source_begin.value, block.text.c_str());
        if (block.task.has_value())
        {
            std::fprintf(stderr, " task(%d,%zu)", block.task.value().checked ? 1 : 0,
                         block.task.value().mark_offset);
        }
        for (const auto &shown : block.spans)
        {
            std::fprintf(stderr, " span(%zu,%zu,%d)", shown.begin.value, shown.end.value,
                         static_cast<int>(shown.style));
        }
        dump_table(block);
        std::fprintf(stderr, "\n");
    }
}

void expect_blocks(std::string_view input, const std::vector<PreviewBlock> &expected,
                   const char *description)
{
    const auto parsed = core::parse_markdown(input, MarkdownStop::never());
    const bool same = parsed.has_value() && parsed.value().blocks == expected;
    expect(same, description);
    if (!same && parsed.has_value())
    {
        dump(parsed.value().blocks);
    }
}

void verify_headings()
{
    expect_blocks("# foo\n## foo\n### foo\n#### foo\n##### foo\n###### foo\n",
                  {titled(1U, "foo", 2U), titled(2U, "foo", 9U), titled(3U, "foo", 17U),
                   titled(4U, "foo", 26U), titled(5U, "foo", 36U), titled(6U, "foo", 47U)},
                  "ATX headings 1 to 6 keep their level and the first text position");
    expect_blocks("####### foo", {leaf(paragraph, "####### foo", 0U)},
                  "seven hashes are a paragraph");
    expect_blocks("#5 bolt\n\n#hashtag",
                  {leaf(paragraph, "#5 bolt", 0U), leaf(paragraph, "#hashtag", 9U)},
                  "a hash without a space is a paragraph");
    expect_blocks("# foo *bar* \\*baz\\*",
                  {styled(titled(1U, "foo bar *baz*", 2U), {span(4U, 7U, emphasis)})},
                  "an ATX heading keeps inline emphasis and escapes");
    expect_blocks("Foo *bar*\n=========\n\nFoo *bar*\n---------",
                  {styled(titled(1U, "Foo bar", 0U), {span(4U, 7U, emphasis)}),
                   styled(titled(2U, "Foo bar", 21U), {span(4U, 7U, emphasis)})},
                  "setext headings are level 1 and 2");
    expect_blocks("#", {titled(1U, "", 0U)}, "an empty heading at the start begins at 0");
}

void verify_paragraphs()
{
    expect_blocks("aaa\n\nbbb", {leaf(paragraph, "aaa", 0U), leaf(paragraph, "bbb", 5U)},
                  "a blank line separates paragraphs");
    expect_blocks("aaa\nbbb", {leaf(paragraph, "aaa bbb", 0U)},
                  "a soft break inside a paragraph becomes one space");
    expect_blocks("  aaa\n bbb", {leaf(paragraph, "aaa bbb", 2U)},
                  "leading spaces are not part of the paragraph");
    expect_blocks("foo  \nbaz", {leaf(paragraph, "foo\nbaz", 0U)},
                  "two trailing spaces are a hard break");
    expect_blocks("foo\\\nbaz", {leaf(paragraph, "foo\nbaz", 0U)},
                  "a backslash at the end of a line is a hard break");
    expect_blocks("\\*not emphasized*", {leaf(paragraph, "*not emphasized*", 1U)},
                  "an escaped first character starts after the backslash");
    expect_blocks("\xE6\x97\xA5\xE6\x9C\xAC\n\xE8\xAA\x9E",
                  {leaf(paragraph, "\xE6\x97\xA5\xE6\x9C\xAC \xE8\xAA\x9E", 0U)},
                  "UTF-8 text passes through and the soft break is a space");
    expect_blocks(std::string_view("a\0b", 3U), {leaf(paragraph, "a�b", 0U)},
                  "a NUL character becomes U+FFFD");
}

void verify_emphasis()
{
    expect_blocks("*foo bar*", {styled(leaf(paragraph, "foo bar", 1U), {span(0U, 7U, emphasis)})},
                  "single stars are emphasis");
    expect_blocks("**foo bar**", {styled(leaf(paragraph, "foo bar", 2U), {span(0U, 7U, strong)})},
                  "double stars are strong");
    expect_blocks("***strong emph***",
                  {styled(leaf(paragraph, "strong emph", 3U),
                          {span(0U, 11U, emphasis), span(0U, 11U, strong)})},
                  "triple stars nest strong inside emphasis, outer span first");
    expect_blocks("*foo **bar** baz*",
                  {styled(leaf(paragraph, "foo bar baz", 1U),
                          {span(0U, 11U, emphasis), span(4U, 7U, strong)})},
                  "strong nested in emphasis");
    expect_blocks("**foo *bar* baz**",
                  {styled(leaf(paragraph, "foo bar baz", 2U),
                          {span(0U, 11U, strong), span(4U, 7U, emphasis)})},
                  "emphasis nested in strong");
    expect_blocks("*foo*bar", {styled(leaf(paragraph, "foobar", 1U), {span(0U, 3U, emphasis)})},
                  "emphasis ends inside a word");
    expect_blocks("a * foo bar*", {leaf(paragraph, "a * foo bar*", 0U)},
                  "a star followed by a space opens no emphasis");
}

void verify_inline_code_and_links()
{
    expect_blocks("`foo`", {styled(leaf(paragraph, "foo", 1U), {span(0U, 3U, inline_code)})},
                  "a code span is styled and located in the source");
    expect_blocks("`` foo ` bar ``",
                  {styled(leaf(paragraph, "foo ` bar", 3U), {span(0U, 9U, inline_code)})},
                  "a double backtick code span keeps a single backtick");
    expect_blocks(
        "[link](/uri \"title\")",
        {linked(styled(leaf(paragraph, "link", 1U), {span(0U, 4U, link)}), {to(0U, 4U, "/uri")})},
        "an inline link keeps only its text");
    expect_blocks("<https://foo.bar.baz>",
                  {linked(styled(leaf(paragraph, "https://foo.bar.baz", 1U), {span(0U, 19U, link)}),
                          {to(0U, 19U, "https://foo.bar.baz")})},
                  "an autolink shows its address");
    expect_blocks(
        "[*a*](u)",
        {linked(styled(leaf(paragraph, "a", 2U), {span(0U, 1U, link), span(0U, 1U, emphasis)}),
                {to(0U, 1U, "u")})},
        "emphasis inside a link");
    // "[画像: " は 9 バイト（[ 1・画像 6・: 1・空白 1）。
    expect_blocks(
        "![foo](/url)",
        {styled(leaf(paragraph, "[\xE7\x94\xBB\xE5\x83\x8F: foo]", 2U), {span(0U, 13U, image)})},
        "an image is shown as its alt text in the image frame");
}

void verify_bullet_lists()
{
    expect_blocks("- one\n- two",
                  {listed(leaf(list_item, "one", 2U), 1U, bullet),
                   listed(leaf(list_item, "two", 8U), 1U, bullet)},
                  "a tight bullet list gives one list item per item");
    expect_blocks("- a\n  - b\n    - c",
                  {listed(leaf(list_item, "a", 2U), 1U, bullet),
                   listed(leaf(list_item, "b", 8U), 2U, bullet),
                   listed(leaf(list_item, "c", 16U), 3U, bullet)},
                  "nested lists deepen list_depth");
    expect_blocks("- a\n\n  b\n- c",
                  {listed(leaf(list_item, "a", 2U), 1U, bullet),
                   listed(leaf(paragraph, "b", 7U), 1U, ""),
                   listed(leaf(list_item, "c", 11U), 1U, bullet)},
                  "a second paragraph in an item is a paragraph without a marker");
    expect_blocks("- foo\n-\n- bar",
                  {listed(leaf(list_item, "foo", 2U), 1U, bullet),
                   listed(leaf(list_item, "", 6U), 1U, bullet),
                   listed(leaf(list_item, "bar", 10U), 1U, bullet)},
                  "an empty item begins on the line after the previous text");
    expect_blocks("- - foo",
                  {listed(leaf(list_item, "", 0U), 1U, bullet),
                   listed(leaf(list_item, "foo", 4U), 2U, bullet)},
                  "an item that only holds a list gives its marker an empty element");
    expect_blocks("- # Foo", {listed(titled(1U, "Foo", 4U), 1U, bullet)},
                  "a heading as the first child of an item takes the marker");
    expect_blocks("- foo\n***\n- bar",
                  {listed(leaf(list_item, "foo", 2U), 1U, bullet), leaf(rule, "", 6U),
                   listed(leaf(list_item, "bar", 12U), 1U, bullet)},
                  "a thematic break between two lists");
}

void verify_ordered_lists()
{
    expect_blocks("1. a\n2. b\n3. c",
                  {listed(leaf(list_item, "a", 3U), 1U, "1."),
                   listed(leaf(list_item, "b", 8U), 1U, "2."),
                   listed(leaf(list_item, "c", 13U), 1U, "3.")},
                  "an ordered list counts from its first number");
    expect_blocks(
        "7. a\n8. b",
        {listed(leaf(list_item, "a", 3U), 1U, "7."), listed(leaf(list_item, "b", 8U), 1U, "8.")},
        "an ordered list may start at 7");
    expect_blocks(
        "3) a\n4) b",
        {listed(leaf(list_item, "a", 3U), 1U, "3)"), listed(leaf(list_item, "b", 8U), 1U, "4)")},
        "a parenthesis delimiter is shown as it is written (#1840)");
    expect_blocks(
        "1) a\n   1. b",
        {listed(leaf(list_item, "a", 3U), 1U, "1)"), listed(leaf(list_item, "b", 11U), 2U, "1.")},
        "a nested list keeps its own delimiter");
    expect_blocks("0. ok", {listed(leaf(list_item, "ok", 3U), 1U, "0.")},
                  "an ordered list may start at 0");
    expect_blocks(
        "1. a\n   - b",
        {listed(leaf(list_item, "a", 3U), 1U, "1."), listed(leaf(list_item, "b", 10U), 2U, bullet)},
        "a bullet list nested in an ordered item");
}

void verify_nested_items()
{
    expect_blocks("- > a", {quoted(listed(leaf(list_item, "a", 4U), 1U, bullet), 1U)},
                  "a quote as the first child of an item takes the marker");
    expect_blocks(
        "1. a\n\n2. b",
        {listed(leaf(list_item, "a", 3U), 1U, "1."), listed(leaf(list_item, "b", 9U), 1U, "2.")},
        "a loose ordered list folds each first paragraph into the item");
    expect_blocks("- ```\n  x\n  ```", {listed(leaf(code, "x\n", 8U), 1U, bullet)},
                  "a code block as the first child of an item keeps its kind and the marker");
    expect_blocks("- <div>\n  x\n  </div>",
                  {listed(leaf(html, "<div>\nx\n</div>\n", 2U), 1U, bullet)},
                  "an HTML block as the first child of an item keeps its kind and the marker");
    const std::string deep = std::string(300U, '>') + " a";
    expect_blocks(deep, {quoted(leaf(paragraph, "a", 301U), 255U)},
                  "a quote depth past 255 saturates");
}

// タスクリスト（ADR 0155 の追補 22・26）。`[ ]` `[x]` `[X]`・入れ子・番号つき・CRLF・印の位置は
// 本文のバイト位置。項目の頭でない `[ ]` と印でない文字はタスクにしない。
void verify_tasks()
{
    expect_blocks("- [ ] a\n- [x] b\n- [X] c",
                  {tasked(listed(leaf(list_item, "a", 6U), 1U, bullet), false, 3U),
                   tasked(listed(leaf(list_item, "b", 14U), 1U, bullet), true, 11U),
                   tasked(listed(leaf(list_item, "c", 22U), 1U, bullet), true, 19U)},
                  "a task item drops its mark from the text and keeps whether it is checked");
    expect_blocks("- [ ] a\n  - [x] b",
                  {tasked(listed(leaf(list_item, "a", 6U), 1U, bullet), false, 3U),
                   tasked(listed(leaf(list_item, "b", 16U), 2U, bullet), true, 13U)},
                  "a nested task item keeps the mark position of its own line");
    expect_blocks("1. [ ] a\n2. [x] b",
                  {tasked(listed(leaf(list_item, "a", 7U), 1U, "1."), false, 4U),
                   tasked(listed(leaf(list_item, "b", 16U), 1U, "2."), true, 13U)},
                  "an ordered task item keeps its number and the mark position");
    expect_blocks("- [ ] a\r\n- [x] b",
                  {tasked(listed(leaf(list_item, "a", 6U), 1U, bullet), false, 3U),
                   tasked(listed(leaf(list_item, "b", 15U), 1U, bullet), true, 12U)},
                  "a CRLF body keeps the mark position in bytes");
    expect_blocks("- a [ ] b", {listed(leaf(list_item, "a [ ] b", 2U), 1U, bullet)},
                  "brackets in the middle of an item are text");
    expect_blocks("a [ ] b\n\n[x] c",
                  {leaf(paragraph, "a [ ] b", 0U), leaf(paragraph, "[x] c", 9U)},
                  "brackets outside a list are text");
    expect_blocks("- [y] a", {listed(leaf(list_item, "[y] a", 2U), 1U, bullet)},
                  "a mark other than a space, x or X is text");
    expect_blocks("- [x] a\n\n  b",
                  {tasked(listed(leaf(list_item, "a", 6U), 1U, bullet), true, 3U),
                   listed(leaf(paragraph, "b", 11U), 1U, "")},
                  "only the first element of a task item holds the task");
}

void verify_quotes()
{
    expect_blocks("> # Foo\n> bar\n> baz",
                  {quoted(titled(1U, "Foo", 4U), 1U), quoted(leaf(paragraph, "bar baz", 10U), 1U)},
                  "a block quote holds a heading and a paragraph");
    expect_blocks("> > > foo", {quoted(leaf(paragraph, "foo", 6U), 3U)},
                  "nested quotes deepen quote_depth");
    expect_blocks("> - a\n> - b",
                  {quoted(listed(leaf(list_item, "a", 4U), 1U, bullet), 1U),
                   quoted(listed(leaf(list_item, "b", 10U), 1U, bullet), 1U)},
                  "a list inside a quote keeps both depths");
    expect_blocks("> bar\nbaz", {quoted(leaf(paragraph, "bar baz", 2U), 1U)},
                  "a lazy continuation line stays in the quote");
}

void verify_code_blocks()
{
    expect_blocks("```\n<\n >\n```", {leaf(code, "<\n >\n", 4U)},
                  "a fenced code block keeps its lines");
    expect_blocks("```ruby\ndef foo(x)\n  return 3\nend\n```",
                  {leaf(code, "def foo(x)\n  return 3\nend\n", 8U)},
                  "an info string is not part of the code");
    expect_blocks("    a simple\n      indented code block",
                  {leaf(code, "a simple\n  indented code block\n", 4U)},
                  "an indented code block keeps the extra indentation");
    expect_blocks("~~~\naaa\n```\n~~~", {leaf(code, "aaa\n```\n", 4U)},
                  "a tilde fence is not closed by backticks");
    expect_blocks("```\n```", {leaf(code, "", 0U)}, "an empty code block at the start begins at 0");
    expect_blocks("a\n\n```\n```", {leaf(paragraph, "a", 0U), leaf(code, "", 3U)},
                  "an empty code block begins on its fence line after the blank line");
}

// 囲みの最初の要素（無ければ空の要素）。
[[nodiscard]] PreviewBlock first_block(std::string_view input)
{
    const auto parsed = core::parse_markdown(input, MarkdownStop::never());
    return parsed.has_value() && !parsed.value().blocks.empty() ? parsed.value().blocks.front()
                                                                : leaf(paragraph, "", 0U);
}

[[nodiscard]] bool colored(const PreviewBlock &block, std::size_t begin, std::size_t end,
                           core::SyntaxRole role)
{
    return block.code_colors.has_value() &&
           std::ranges::find(block.code_colors.value(),
                             core::PreviewCodeColor{Offset{begin}, Offset{end}, role}) !=
               block.code_colors.value().end();
}

// 切れ 3 つ（first・middle・last）になる囲みの行の数（切れの行の数の 2 倍と 44 行・128 なら 300）。
[[nodiscard]] std::size_t three_piece_lines() noexcept
{
    return (2U * core::preview_code_piece_lines) + 44U;
}

[[nodiscard]] std::string fenced_lines(std::string_view language, std::size_t lines,
                                       std::string_view row = "x = 1")
{
    std::string input = "```" + std::string(language) + "\n";
    for (std::size_t line = 0; line < lines; ++line)
    {
        input += row;
        input += "\n";
    }
    return input + "```\n";
}

// 1 行に count 個の数（py の数の区間が count 個・間の空白で畳まない）の囲みの本文。
[[nodiscard]] std::string numbers_line(std::size_t count)
{
    std::string line;
    for (std::size_t at = 0; at < count; ++at)
    {
        line += at == 0U ? "1" : " 1";
    }
    return line;
}

// 区間の数の上限（#1860 のレビュー M1・切れごと・#1863）と、隣り合う同じ役の区間を畳むこと。
void verify_code_color_count()
{
    const auto limit = core::preview_code_color_count_limit;
    const auto full = first_block("```py\n" + numbers_line(limit) + "\n```\n");
    expect(full.code_colors.has_value() &&
               full.code_colors.value_or(std::vector<core::PreviewCodeColor>{}).size() == limit,
           "a piece with as many colours as the limit is coloured");
    expect(!first_block("```py\n" + numbers_line(limit + 1U) + "\n```\n").code_colors.has_value(),
           "a piece with more colours than the limit keeps one colour");
    const auto parsed = core::parse_markdown(fenced_lines("json", 1000U, "[1,2,3,4,5,6,7,8,9]"),
                                             MarkdownStop::never());
    expect(parsed.has_value() &&
               parsed.value().blocks.size() ==
                   (1000U + core::preview_code_piece_lines - 1U) / core::preview_code_piece_lines &&
               std::ranges::all_of(parsed.value().blocks, [](const PreviewBlock &block)
                                   { return block.code_colors.has_value(); }),
           "a dense JSON fence over the colour limit keeps its colours piece by piece");
    const auto joined = first_block("```py\n\"a\"\"b\" \"c\"\n```");
    expect(joined.code_colors == std::optional{std::vector<core::PreviewCodeColor>{
                                     {Offset{0U}, Offset{6U}, core::SyntaxRole::string},
                                     {Offset{7U}, Offset{10U}, core::SyntaxRole::string}}},
           "adjacent spans of one role are joined, spans across a space are not");
}

// 囲みの始まりの状態（#1860 のレビュー R3）と、引用・リスト・CRLF・Tab・行をまたぐ文字列。
void verify_code_color_places()
{
    expect(colored(first_block("```php\n$x = 1; // c\n```"), 8U, 12U, core::SyntaxRole::comment),
           "a php fence without <?php is read as php");
    for (const auto name : core::all_language_names())
    {
        const auto language = core::language_of_name(name).value_or(core::Language::plain);
        expect(language == core::Language::php ||
                   core::lex_start_state(language, false) == core::lex_start_state(language, true),
               "only php starts a fence in another state");
    }
    expect(colored(first_block("```rust,ignore\n// c\n```"), 0U, 4U, core::SyntaxRole::comment),
           "a comma ends the language word");
    for (const std::string_view input :
         {"```py\r\n# c\r\n```\r\n", "> ```py\n> # c\n> ```", "- ```py\n  # c\n  ```"})
    {
        expect(colored(first_block(input), 0U, 3U, core::SyntaxRole::comment),
               "a fence after CRLF, in a quote or in an item colours its own text");
    }
    // md4c は行頭の Tab を空白 4 つにして渡すので、色は要素の text（空白）の位置に立つ。
    const auto tabbed = first_block("```py\n\tx = 1  # c\n```");
    expect(tabbed.text == "    x = 1  # c\n" &&
               colored(tabbed, 11U, 14U, core::SyntaxRole::comment),
           "a leading tab keeps the colours on the element text");
    const auto raw = first_block("```cpp\nR\"(a\nb)\" x\n```");
    expect(colored(raw, 0U, 4U, core::SyntaxRole::string) &&
               colored(raw, 5U, 8U, core::SyntaxRole::string),
           "a raw string carries over the line");
    const auto triple = first_block("```py\n\"\"\"a\nb\"\"\"\n```");
    expect(colored(triple, 0U, 4U, core::SyntaxRole::string) &&
               colored(triple, 5U, 9U, core::SyntaxRole::string),
           "a triple quoted string carries over the line");
}

// 囲みのコードの構文の色（#1858）。言語は info string の最初の語で本文と同じ表から引き、色の区間
// は本文と同じ字句器の答えを要素の text のバイト位置で持つ。
void verify_code_colors()
{
    const auto cpp = first_block("```cpp\nint x; // c\n```");
    expect(cpp.kind == code && cpp.text == "int x; // c\n" && cpp.source_begin == Offset{7U} &&
               colored(cpp, 7U, 11U, core::SyntaxRole::comment),
           "a cpp fence colours the comment with the body lexer");
    expect(cpp.code_colors ==
               core::preview_code_piece_colors(core::Language::cpp, cpp.text,
                                               core::preview_code_piece_starts(cpp.text))
                   .front(),
           "the colours are the lexer's answer for the code text");
    const auto python = first_block("```Python\n\"\xE3\x81\x82\" # x\n```");
    expect(colored(python, 0U, 5U, core::SyntaxRole::string) &&
               colored(python, 6U, 9U, core::SyntaxRole::comment),
           "a folded name colours by byte offsets past a multibyte character");
    const auto comment = first_block("```c++ title=\"x\"\n/* a\nb */ 1\n```");
    expect(colored(comment, 0U, 4U, core::SyntaxRole::comment) &&
               colored(comment, 5U, 9U, core::SyntaxRole::comment),
           "an alias with more words carries a block comment over the line");
    for (const std::string_view input :
         {"```zig\nconst x = 1;\n```", "```\nint x;\n```", "    int x;\n", "```markdown\n# a\n```",
          "```md\n# a\n```", "```text\nint x;\n```", "```{.cpp}\nint x;\n```"})
    {
        const auto block = first_block(input);
        expect(block.kind == code && !block.code_colors.has_value(),
               "an unknown, missing, markdown or plain language keeps one colour");
    }
    expect(first_block(fenced_lines("py", core::preview_code_color_line_limit, "x"))
               .code_colors.has_value(),
           "a fence at the line limit is coloured");
    expect(!first_block(fenced_lines("py", core::preview_code_color_line_limit + 1U, "x"))
                .code_colors.has_value(),
           "a fence over the line limit keeps one colour");
    const std::string wide =
        "```py\n" + std::string(core::preview_code_color_byte_limit, 'a') + "\n```\n";
    expect(!first_block(wide).code_colors.has_value(),
           "a fence over the byte limit keeps one colour");
}

// 文書の要素（読めなければ空の列）。
[[nodiscard]] std::vector<PreviewBlock> blocks_of(std::string_view input)
{
    auto parsed = core::parse_markdown(input, MarkdownStop::never());
    return parsed.has_value() ? std::move(parsed).value().blocks : std::vector<PreviewBlock>{};
}

[[nodiscard]] std::string repeated(std::string_view row, std::size_t count)
{
    std::string text;
    for (std::size_t at = 0; at < count; ++at)
    {
        text += row;
    }
    return text;
}

[[nodiscard]] bool pieces_are(const std::vector<PreviewBlock> &blocks,
                              const std::vector<core::PreviewCodePiece> &pieces)
{
    return blocks.size() == pieces.size() &&
           std::ranges::equal(blocks, pieces, {}, &PreviewBlock::piece) &&
           std::ranges::all_of(blocks,
                               [](const PreviewBlock &block) { return block.kind == code; });
}

// 長い囲みは preview_code_piece_lines 行ごとの切れの列（ADR 0151 の追補 2026-11-19・#1863）。
void verify_code_pieces()
{
    using enum core::PreviewCodePiece;
    const auto lines = core::preview_code_piece_lines;
    const auto exact = blocks_of(fenced_lines("py", lines));
    expect(pieces_are(exact, {only}) && exact.front().text == repeated("x = 1\n", lines),
           "a fence of exactly the piece lines is one element");
    const auto over = blocks_of(fenced_lines("py", lines + 1U));
    expect(pieces_are(over, {first, last}) && over.front().text == repeated("x = 1\n", lines) &&
               over.back().text == "x = 1\n" && over.front().source_begin == Offset{6U} &&
               over.back().source_begin == Offset{6U + (lines * 6U)},
           "a fence one line over is two pieces and the second begins on its first line");
    const auto three = blocks_of(fenced_lines("py", three_piece_lines()));
    expect(pieces_are(three, {first, middle, last}) &&
               three.at(2).text == repeated("x = 1\n", three_piece_lines() - (2U * lines)),
           "a fence of two pieces and some is a first, a middle and a last piece");
    const auto starts = [](const std::string &text)
    { return core::preview_code_piece_starts(text); };
    const std::vector<std::size_t> one{0U};
    const std::vector<std::size_t> two{0U, 2U * lines};
    expect(starts("") == one && starts(repeated("a\n", lines)) == one &&
               starts(repeated("a\n", lines) + "a") == two &&
               starts(repeated("a\n", lines + 1U)) == two,
           "pieces start every piece lines and no empty piece follows the last newline");
    const auto front_matter = blocks_of("---\n" + repeated("a: 1\n", 200U) + "---\n");
    expect(front_matter.size() == 1U && front_matter.front().kind == PreviewBlockKind::front_matter,
           "a long front matter is not split");
    const auto indented = blocks_of(repeated("    x\n", lines + 1U));
    expect(pieces_are(indented, {first, last}) &&
               indented.back().source_begin == Offset{(lines * 6U) + 4U},
           "an indented code block is split the same way");
}

// 切れの位置は元の行の位置（引用・リスト・CRLF・空の行）。
void verify_code_piece_places()
{
    const auto lines = core::preview_code_piece_lines;
    const auto quoted = blocks_of("> ```py\n" + repeated("> x\n", lines + 1U) + "> ```\n");
    expect(quoted.size() == 2U && quoted.back().quote_depth == 1U &&
               quoted.back().source_begin == Offset{8U + (lines * 4U) + 2U},
           "a piece in a quote begins after the quote mark of its first line");
    const auto item = blocks_of("- ```py\n" + repeated("  x\n", lines + 1U) + "  ```\n");
    expect(item.size() == 2U && item.front().marker == bullet && item.back().marker.empty() &&
               item.back().list_depth == 1U &&
               item.back().source_begin == Offset{8U + (lines * 4U) + 2U},
           "only the first piece of a fence in an item carries the marker");
    const auto crlf = blocks_of("```py\r\n" + repeated("x\r\n", lines + 1U) + "```\r\n");
    expect(crlf.size() == 2U && crlf.back().source_begin == Offset{7U + (lines * 3U)},
           "a CRLF fence counts CRLF as one line end");
    const auto blank = blocks_of("```\n" + repeated("x\n", lines) + "\nx\n```\n");
    expect(blank.size() == 2U && blank.back().text == "\nx\n" &&
               blank.back().source_begin == Offset{4U + (lines * 2U)},
           "a piece that starts on a blank line begins at that line");
    const auto empty = blocks_of("```\n" + repeated("\n", lines + 1U) + "```\n");
    expect(empty.size() == 2U && empty.back().source_begin == empty.front().source_begin,
           "a fence of blank lines keeps the pieces in order");
}

// 切れの列の写しは元の囲みと同じ文字列で、本文の位置は行を持つ切れへ写る。
void verify_code_piece_copy()
{
    auto parsed =
        core::parse_markdown(fenced_lines("py", three_piece_lines()), MarkdownStop::never());
    if (!parsed.has_value())
    {
        expect(false, "the three piece fence parses");
        return;
    }
    const auto &document = parsed.value();
    const auto all = core::preview_selection_all(document);
    auto whole = repeated("x = 1\n", three_piece_lines());
    whole.pop_back();
    expect(all.has_value() && core::preview_selection_text(document, all.value()) == whole,
           "copying every piece gives the fence text with one newline between pieces");
    expect(
        core::preview_anchor_block(document, Offset{6U + ((core::preview_code_piece_lines +
                                                           (core::preview_code_piece_lines / 2U)) *
                                                          6U)}) == 1U &&
            core::preview_anchor_block(
                document, Offset{6U + ((core::preview_code_piece_lines - 1U) * 6U)}) == 0U,
        "a body position maps to the piece that holds its line");
}

// Shift+上下と Home / End は切れの列を 1 つの囲みとして動く（#1877 のレビュー R2）。文書は段落 a・
// 切れ 3 つの囲み（切れ 1〜3）・段落 b。
void verify_code_piece_motion()
{
    auto parsed = core::parse_markdown("a\n\n" + fenced_lines("py", three_piece_lines()) + "\nb\n",
                                       MarkdownStop::never());
    if (!parsed.has_value() || parsed.value().blocks.size() != 5U)
    {
        expect(false, "the fence between two paragraphs parses into five elements");
        return;
    }
    const auto &document = parsed.value();
    const auto moved = [&document](std::size_t block, std::size_t offset, core::CaretMotion motion)
    { return core::preview_point_moved(document, core::PreviewPoint{block, offset}, motion); };
    const core::PreviewPoint head{1U, 0U};
    const core::PreviewPoint end{3U, core::preview_shown_text(document.blocks.at(3)).size()};
    expect(moved(2U, 5U, core::CaretMotion::line_start) == head &&
               moved(2U, 5U, core::CaretMotion::line_end) == end,
           "Home and End go to the head and the end of the whole fence");
    expect(moved(2U, 5U, core::CaretMotion::previous_line) == head &&
               moved(2U, 0U, core::CaretMotion::previous_line) == head &&
               moved(1U, 0U, core::CaretMotion::previous_line) == core::PreviewPoint{0U, 0U},
           "up goes to the head of the fence, then to the element before it");
    expect(moved(2U, 5U, core::CaretMotion::next_line) == end &&
               moved(3U, end.offset, core::CaretMotion::next_line) == core::PreviewPoint{4U, 1U} &&
               moved(4U, 0U, core::CaretMotion::previous_line) == head,
           "down goes to the end of the fence, then to the end of the element after it");
    expect(moved(4U, 0U, core::CaretMotion::line_end) == core::PreviewPoint{4U, 1U} &&
               moved(0U, 1U, core::CaretMotion::next_line) == end &&
               moved(4U, 1U, core::CaretMotion::line_start) == core::PreviewPoint{4U, 0U},
           "an element that is not a piece moves as before and reaches the fence as one");
}

// 色は囲み全体で 1 回読み、切れの text の位置に切り分ける。行をまたぐ状態は境を越え、区間の上限は
// 切れごと（超えた切れだけ 1 色）。
void verify_code_piece_colors()
{
    const auto lines = core::preview_code_piece_lines;
    const auto three = blocks_of(fenced_lines("py", three_piece_lines(), "x = 1  # c"));
    const auto rows = repeated("x = 1  # c\n", three_piece_lines());
    const auto expected = core::preview_code_piece_colors(core::Language::python, rows,
                                                          core::preview_code_piece_starts(rows));
    expect(three.size() == 3U && expected.size() == 3U &&
               std::ranges::equal(three, expected, {}, &PreviewBlock::code_colors) &&
               three.at(1).code_colors == three.at(0).code_colors &&
               colored(three.at(1), 7U, 10U, core::SyntaxRole::comment),
           "each piece holds the colours at the positions of its own text");
    const auto comment =
        blocks_of("```cpp\n" + repeated("int x;\n", lines - 1U) + "/* a\nb */ 1\n```\n");
    expect(comment.size() == 2U && colored(comment.back(), 0U, 4U, core::SyntaxRole::comment),
           "a block comment carries over the piece boundary");
    const auto raw =
        blocks_of("```cpp\n" + repeated("int x;\n", lines - 1U) + "R\"(a\nb)\" x\n```\n");
    expect(raw.size() == 2U && colored(raw.back(), 0U, 3U, core::SyntaxRole::string),
           "a raw string carries over the piece boundary");
    const auto triple =
        blocks_of("```py\n" + repeated("x = 1\n", lines - 1U) + "\"\"\"a\nb\"\"\"\n```\n");
    expect(triple.size() == 2U && colored(triple.back(), 0U, 4U, core::SyntaxRole::string),
           "a triple quoted string carries over the piece boundary");
    const auto open = blocks_of("```py\n" + repeated("x = 1  # c\n", lines + 1U));
    expect(pieces_are(open, {core::PreviewCodePiece::first, core::PreviewCodePiece::last}) &&
               colored(open.back(), 7U, 10U, core::SyntaxRole::comment),
           "a fence left open at the end of the document is split and coloured");
    const auto crowded = blocks_of("```cpp\n" + repeated("int x;\n", lines) +
                                   numbers_line(core::preview_code_color_count_limit + 1U) +
                                   "\n/* a\n" + repeated("int x;\n", lines - 2U) + "b */ 1\n```\n");
    expect(crowded.size() == 3U && crowded.at(0).code_colors.has_value() &&
               !crowded.at(1).code_colors.has_value() &&
               colored(crowded.at(2), 0U, 4U, core::SyntaxRole::comment),
           "only the piece over the colour limit keeps one colour and the state still carries");
    const auto unknown = blocks_of(fenced_lines("zig", lines + 1U));
    expect(unknown.size() == 2U && !unknown.front().code_colors.has_value() &&
               !unknown.back().code_colors.has_value(),
           "pieces of an unknown language keep one colour");
    const auto wide = blocks_of(fenced_lines("py", core::preview_code_color_line_limit + 1U));
    expect(!wide.empty() && std::ranges::none_of(wide, [](const PreviewBlock &block)
                                                 { return block.code_colors.has_value(); }),
           "a fence over the line limit keeps one colour in every piece");
}

void verify_rules()
{
    expect_blocks("***\n---\n___", {leaf(rule, "", 0U), leaf(rule, "", 0U), leaf(rule, "", 0U)},
                  "thematic breaks with no text before them begin at 0");
    expect_blocks("Foo\n***\nbar",
                  {leaf(paragraph, "Foo", 0U), leaf(rule, "", 4U), leaf(paragraph, "bar", 8U)},
                  "a thematic break begins on the line after the previous text");
    expect_blocks("+++", {leaf(paragraph, "+++", 0U)}, "plus signs are not a thematic break");
    expect_blocks("para\n\n---", {leaf(paragraph, "para", 0U), leaf(rule, "", 6U)},
                  "a thematic break after a blank line begins on its own line");
    expect_blocks("a\n\n  \t\r\n***", {leaf(paragraph, "a", 0U), leaf(rule, "", 8U)},
                  "lines of only spaces, tabs and CR are skipped as blank");
    expect_blocks("\n\n---", {leaf(rule, "", 2U)},
                  "a thematic break after leading blank lines skips them");
}

void verify_entities()
{
    expect_blocks("&nbsp; &amp; &copy; &AElig; &Dcaron;\n&frac34; &HilbertSpace; "
                  "&DifferentialD;\n&ClockwiseContourIntegral; &ngE;",
                  {leaf(paragraph, "  & © Æ Ď ¾ ℋ ⅆ ∲ ≧̸", 0U)},
                  "named entities from the CommonMark examples are resolved");
    expect_blocks("&#35; &#1234; &#992; &#0;", {leaf(paragraph, "# Ӓ Ϡ �", 0U)},
                  "decimal references are resolved and 0 is U+FFFD");
    expect_blocks("&#X22; &#XD06; &#xcab;", {leaf(paragraph, "\" ആ ಫ", 0U)},
                  "hexadecimal references are resolved");
    expect_blocks("&#xD800; &#x110000;", {leaf(paragraph, "� �", 0U)},
                  "surrogates and values past U+10FFFF are U+FFFD");
    expect_blocks(
        "&nbsp &x; &#; &#x;\n&#87654321;\n&#abcdef0;\n&ThisIsNotDefined; &hi?;",
        {leaf(paragraph, "&nbsp &x; &#; &#x; &#87654321; &#abcdef0; &ThisIsNotDefined; &hi?;", 0U)},
        "unknown names and malformed references keep their spelling");
    expect_blocks("&copy", {leaf(paragraph, "&copy", 0U)},
                  "a reference without a semicolon is text");
    expect_blocks("&#42;foo&#42;", {leaf(paragraph, "*foo*", 0U)},
                  "a resolved star does not open emphasis");
    expect_blocks("**&amp;**", {styled(leaf(paragraph, "&", 2U), {span(0U, 1U, strong)})},
                  "an entity inside strong sets the span and the position");
    expect_blocks("[a](url &quot;tit&quot;)", {leaf(paragraph, "[a](url \"tit\")", 0U)},
                  "an entity quote does not delimit a link title");
}

// HTML5 の名前つきの実体参照の全表（#1840・生成物 MarkdownEntityTable.hpp）。2 コードポイントに写る
// 名前・大文字の別名・表に無い名前・`;` の無い綴り。
void verify_html5_entities()
{
    expect_blocks("&hellip; &mdash; &ndash; &rarr; &AMP; &NotEqualTilde;",
                  {leaf(paragraph, "… — – → & ≂̸", 0U)},
                  "HTML5 named references are resolved, two code points included");
    expect_blocks("&AElig;&zwnj;", {leaf(paragraph, "Æ\xE2\x80\x8C", 0U)},
                  "the first and last names of the table are found by the binary search");
    expect_blocks("&nosuch; &amp &hellip &Hellip;",
                  {leaf(paragraph, "&nosuch; &amp &hellip &Hellip;", 0U)},
                  "unknown names and names without a semicolon keep their spelling");
}

// 打ち消し線（MD_FLAG_STRIKETHROUGH・#1840）。記号は文字に入らず、装飾が範囲を持つ。打ち消しは
// `~~` だけで、md4c が打ち消しにする `~` 1 つの対は文字のまま元の位置に戻す（日本語の範囲の `~`・
// #1840 のレビュー）。3 つ以上と数の合わない対も文字のまま。
void verify_strikethrough()
{
    const PreviewStyle struck = PreviewStyle::strikethrough;
    expect_blocks("~~a~~", {styled(leaf(paragraph, "a", 2U), {span(0U, 1U, struck)})},
                  "double tildes strike through and drop the marks");
    expect_blocks("~a~", {leaf(paragraph, "~a~", 1U)}, "single tildes are text");
    expect_blocks("10時~12時、14時~16時", {leaf(paragraph, "10時~12時、14時~16時", 0U)},
                  "tildes for ranges keep the text between them as it is");
    expect_blocks("*~a~*", {styled(leaf(paragraph, "~a~", 2U), {span(0U, 3U, emphasis)})},
                  "single tildes inside emphasis keep the emphasis around them");
    expect_blocks("~a **b** c~", {styled(leaf(paragraph, "~a b c~", 1U), {span(3U, 4U, strong)})},
                  "strong inside single tildes moves by the opening tilde");
    expect_blocks("~**b** c~", {styled(leaf(paragraph, "~b c~", 3U), {span(1U, 2U, strong)})},
                  "strong right after a single tilde moves by the tilde");
    expect_blocks("~~a ~b~ c~~", {styled(leaf(paragraph, "a ~b~ c", 2U), {span(0U, 7U, struck)})},
                  "single tildes inside a strike through are text inside it");
    expect_blocks("~a~\nb~c~", {leaf(paragraph, "~a~ b~c~", 1U)},
                  "single tildes at the start and the end of lines are text");
    expect_blocks("\\~a~", {leaf(paragraph, "~a~", 1U)},
                  "an escaped tilde and a lone tilde are text");
    expect_blocks("`~a~`", {styled(leaf(paragraph, "~a~", 1U), {span(0U, 3U, inline_code)})},
                  "single tildes in a code span are text");
    expect_blocks("\\~~a~ b", {leaf(paragraph, "~~a~ b", 1U)},
                  "an escaped tilde before a single tilde pair is not counted as a mark");
    expect_blocks("x\\~~a~", {leaf(paragraph, "x~~a~", 0U)},
                  "an escaped tilde after text before a single tilde pair keeps every tilde");
    expect_blocks("x ~~~a~~~ y", {leaf(paragraph, "x ~~~a~~~ y", 0U)}, "three tildes are text");
    expect_blocks("x ~~a~ y", {leaf(paragraph, "x ~~a~ y", 0U)},
                  "tildes that do not pair are text");
    expect_blocks("**~~a~~**",
                  {styled(leaf(paragraph, "a", 4U), {span(0U, 1U, strong), span(0U, 1U, struck)})},
                  "strike through nested in strong");
    expect_blocks(
        "~~a **b** c~~",
        {styled(leaf(paragraph, "a b c", 2U), {span(0U, 5U, struck), span(2U, 3U, strong)})},
        "strong nested in strike through");
    expect_blocks("`~~a~~`", {styled(leaf(paragraph, "~~a~~", 1U), {span(0U, 5U, inline_code)})},
                  "tildes in a code span are text");
    expect_blocks("```\n~~a~~\n```", {leaf(code, "~~a~~\n", 4U)},
                  "tildes in a code block are text");
}

// 裸の URL・www・メール（MD_FLAG_PERMISSIVE{URL,WWW,EMAIL}AUTOLINKS・#1840）。
// 見た目は今のリンクと同じ装飾で、文字は書いたまま（http:// を足さない）。
// 末尾の句読点はリンクに入らない。
void verify_bare_links()
{
    expect_blocks("see https://example.com/a?b=1.",
                  {linked(styled(leaf(paragraph, "see https://example.com/a?b=1.", 0U),
                                 {span(4U, 29U, link)}),
                          {to(4U, 29U, "https://example.com/a?b=1")})},
                  "a bare URL is a link without its trailing period");
    expect_blocks("www.example.com",
                  {linked(styled(leaf(paragraph, "www.example.com", 0U), {span(0U, 15U, link)}),
                          {to(0U, 15U, "http://www.example.com")})},
                  "a www address is a link shown as written");
    expect_blocks("mail a@example.com",
                  {linked(styled(leaf(paragraph, "mail a@example.com", 0U), {span(5U, 18U, link)}),
                          {to(5U, 18U, "mailto:a@example.com")})},
                  "an e-mail address is a link");
    expect_blocks("example.com", {leaf(paragraph, "example.com", 0U)},
                  "a host without a scheme or www is text");
    expect_blocks(
        "`https://example.com`",
        {styled(leaf(paragraph, "https://example.com", 1U), {span(0U, 19U, inline_code)})},
        "an address in a code span is not a link");
}

[[nodiscard]] PreviewBlock front(std::string text)
{
    return leaf(PreviewBlockKind::front_matter, std::move(text), 0U);
}

// 文書の先頭の front matter（#1840）。`---` の行から `---` か `...` の行までを
// Markdown として読まず 1 要素にし、後ろの要素の位置と task の印の位置は
// 本文の中の位置のまま。閉じる行が無い・先頭の行が `---` でない・
// 先頭に空行がある文書は今までどおり。
void verify_front_matter()
{
    expect_blocks("---\ntitle: a\n---\n# b", {front("title: a\n"), titled(1U, "b", 19U)},
                  "a front matter closed by --- is one element and the heading keeps its position");
    expect_blocks("---\na: 1\n...\nx", {front("a: 1\n"), leaf(paragraph, "x", 13U)},
                  "a front matter may be closed by ...");
    expect_blocks("---\r\na: 1\r\nb: 2\r\n---\r\nx",
                  {front("a: 1\nb: 2\n"), leaf(paragraph, "x", 22U)},
                  "a CRLF front matter ends its lines with LF and keeps byte positions");
    expect_blocks("---\na\n---", {front("a\n")}, "a front matter may end the document");
    expect_blocks("---\n---\nx", {front(""), leaf(paragraph, "x", 8U)},
                  "an empty front matter is one empty element");
    expect_blocks("---\n# not a heading\n- not a list\n---\n",
                  {front("# not a heading\n- not a list\n")},
                  "the inside of a front matter is not read as Markdown");
    expect_blocks("---\na\n---\n\n***", {front("a\n"), leaf(rule, "", 11U)},
                  "an element without text after a front matter begins on its own line");
    expect_blocks("---\na\n---\n- [x] b",
                  {front("a\n"), tasked(listed(leaf(list_item, "b", 16U), 1U, bullet), true, 13U)},
                  "a task mark after a front matter keeps its position in the document");
    expect_blocks("---\na: 1\n", {leaf(rule, "", 0U), leaf(paragraph, "a: 1", 4U)},
                  "without a closing line the document is read as before");
    expect_blocks("\n---\na\n---\n", {leaf(rule, "", 1U), titled(2U, "a", 5U)},
                  "a blank line before the opening line reads as before");
    expect_blocks("----\na\n----", {leaf(rule, "", 0U), titled(2U, "a", 5U)},
                  "an opening line other than --- reads as before");
    expect_blocks("--- \na\n---\t\nb", {front("a\n"), leaf(paragraph, "b", 12U)},
                  "spaces and tabs after the opening and closing lines are allowed (Jekyll)");
    expect_blocks(std::string_view("---\na\0b\n---\nx", 13U),
                  {front("a\xEF\xBF\xBD"
                         "b\n"),
                   leaf(paragraph, "x", 12U)},
                  "a NUL in a front matter becomes U+FFFD like the body");
    expect_blocks("---", {leaf(rule, "", 0U)}, "a lone --- is a thematic break");
}

// 表（MD_FLAG_TABLES・#1848・D103）。行ごとに要素 table_row を 1 つ・升の間は Tab・位置は行ごと。
void verify_tables()
{
    using enum core::PreviewTableAlign;
    expect_blocks("| a | b |\n|---|--:|\n| 1 | 22 |",
                  {rowed("a\tb", 2U, 0U, {{0U, left}, {2U, right}}),
                   rowed("1\t22", 22U, 1U, {{0U, left}, {2U, right}})},
                  "a table gives one element per row with the cells and the alignments");
    expect_blocks(
        "| l | c | r | d |\n|:--|:-:|--:|---|\n| 1 | 2 | 3 | 4 |",
        {rowed("l\tc\tr\td", 2U, 0U, {{0U, left}, {2U, center}, {4U, right}, {6U, left}}),
         rowed("1\t2\t3\t4", 38U, 1U, {{0U, left}, {2U, center}, {4U, right}, {6U, left}})},
        "colons set the left, center and right alignment and none is left");
    expect_blocks("| *a* | `b` |\n|---|---|\n| ~~c~~ | [d](u) |",
                  {styled(rowed("a\tb", 3U, 0U, {{0U, left}, {2U, left}}),
                          {span(0U, 1U, emphasis), span(2U, 3U, inline_code)}),
                   linked(styled(rowed("c\td", 28U, 1U, {{0U, left}, {2U, left}}),
                                 {span(0U, 1U, PreviewStyle::strikethrough), span(2U, 3U, link)}),
                          {to(2U, 3U, "u")})},
                  "spans inside cells keep their style in the row text");
    expect_blocks("| a \\| b |\n|---|\n| c |",
                  {rowed("a | b", 2U, 0U, {{0U, left}}), rowed("c", 19U, 1U, {{0U, left}})},
                  "an escaped pipe is text inside a cell");
    expect_blocks("| a | b |\n|---|---|\n| 1 |\n| 1 | 2 | 3 |",
                  {rowed("a\tb", 2U, 0U, {{0U, left}, {2U, left}}),
                   rowed("1\t", 22U, 1U, {{0U, left}, {2U, left}}),
                   rowed("1\t2", 28U, 2U, {{0U, left}, {2U, left}})},
                  "a short row gets empty cells and a long row drops the extra cells");
    expect_blocks("x\n\n| a |\n|---|\n| 1 |\n\ny",
                  {leaf(paragraph, "x", 0U), rowed("a", 5U, 0U, {{0U, left}}),
                   rowed("1", 17U, 1U, {{0U, left}}), leaf(paragraph, "y", 22U)},
                  "paragraphs before and after a table stay paragraphs");
    expect_blocks(
        "| a |\n|---|\n\n| b |\n|---|",
        {rowed("a", 2U, 0U, {{0U, left}}), numbered(rowed("b", 15U, 0U, {{0U, left}}), 1U)},
        "a second table counts its rows from 0 again and has the next ordinal");
    expect_blocks("| a&NewLine;b |\n|---|", {rowed("a b", 2U, 0U, {{0U, left}})},
                  "a newline from an entity in a cell becomes a space");
    expect_blocks("| a |\n| b |", {leaf(paragraph, "| a | | b |", 0U)},
                  "rows without a delimiter row are not a table");
}

// GitHub の alert（#1848・D103）。いちばん外の引用の最初の行が `[!NOTE]` などだけのとき、印を種類の
// 名前と改行に置き換えて名前に alert_title を足し、引用の中の要素に種類を置く。
void verify_alerts()
{
    using enum core::PreviewAlert;
    const PreviewStyle title = PreviewStyle::alert_title;
    expect_blocks(
        "> [!NOTE]\n> body",
        {alerted(styled(quoted(leaf(paragraph, "NOTE\nbody", 2U), 1U), {span(0U, 4U, title)}),
                 note)},
        "a note alert shows its name on the first line");
    expect_blocks(
        "> [!tip]\n> x",
        {alerted(styled(quoted(leaf(paragraph, "TIP\nx", 2U), 1U), {span(0U, 3U, title)}), tip)},
        "the marker is matched without case and the name is upper case");
    expect_blocks(
        ">  [!Important]  \n> x",
        {alerted(styled(quoted(leaf(paragraph, "IMPORTANT\nx", 3U), 1U), {span(0U, 9U, title)}),
                 important)},
        "spaces around the marker are allowed");
    expect_blocks(
        "> [!WARNING]",
        {alerted(styled(quoted(leaf(paragraph, "WARNING", 2U), 1U), {span(0U, 7U, title)}),
                 warning)},
        "an alert with only the marker shows only the name");
    expect_blocks("> [!CAUTION]\n> *a*\n>\n> - b",
                  {alerted(styled(quoted(leaf(paragraph, "CAUTION\na", 2U), 1U),
                                  {span(0U, 7U, title), span(8U, 9U, emphasis)}),
                           caution),
                   alerted(quoted(listed(leaf(list_item, "b", 25U), 1U, bullet), 1U), caution)},
                  "every element of the alert quote keeps the kind and later spans move");
    expect_blocks("> [!NOTE] extra\n> body",
                  {quoted(leaf(paragraph, "[!NOTE] extra body", 2U), 1U)},
                  "text after the marker on the first line is an ordinary quote");
    expect_blocks("> [!NOPE]\n> body", {quoted(leaf(paragraph, "[!NOPE] body", 2U), 1U)},
                  "an unknown kind is an ordinary quote");
    expect_blocks("> > [!NOTE]\n> > x", {quoted(leaf(paragraph, "[!NOTE] x", 4U), 2U)},
                  "an alert marker in a nested quote is an ordinary quote");
    expect_blocks("- > [!NOTE]\n  > x",
                  {quoted(listed(leaf(list_item, "[!NOTE] x", 4U), 1U, bullet), 1U)},
                  "an alert marker in a quote inside a list is an ordinary quote");
    expect_blocks("> # [!NOTE]", {quoted(titled(1U, "[!NOTE]", 4U), 1U)},
                  "a heading first is an ordinary quote");
    expect_blocks("> \\[!NOTE]\n> body", {quoted(leaf(paragraph, "[!NOTE] body", 3U), 1U)},
                  "an escaped marker is an ordinary quote with the marker as text");
    expect_blocks(
        "> [!NOTE]\n> a\n\n> b",
        {alerted(styled(quoted(leaf(paragraph, "NOTE\na", 2U), 1U), {span(0U, 4U, title)}), note),
         quoted(leaf(paragraph, "b", 17U), 1U)},
        "the next quote after a blank line is a new ordinary quote");
}

// リンクの宛先の列（ADR 0151 の追補 2026-11-19・#1869）。範囲は link の装飾と同じで、宛先は
// 実体参照と `\` のエスケープを解いた href（`%` は解かない）。画像の宛先は載らない。
void verify_link_targets()
{
    expect_blocks("[a][1]\n\n[1]: http://x.y/z \"t\"",
                  {linked(styled(leaf(paragraph, "a", 1U), {span(0U, 1U, link)}),
                          {to(0U, 1U, "http://x.y/z")})},
                  "a reference link takes the destination of its definition");
    expect_blocks(
        "[a](1) [b](2)",
        {linked(styled(leaf(paragraph, "a b", 1U), {span(0U, 1U, link), span(2U, 3U, link)}),
                {to(0U, 1U, "1"), to(2U, 3U, "2")})},
        "links are listed in the order they open");
    expect_blocks(
        "[a](b\\_c&amp;d%20e)",
        {linked(styled(leaf(paragraph, "a", 1U), {span(0U, 1U, link)}), {to(0U, 1U, "b_c&d%20e")})},
        "escapes and entities are resolved and percent signs are kept");
    expect_blocks("<a@example.com>",
                  {linked(styled(leaf(paragraph, "a@example.com", 1U), {span(0U, 13U, link)}),
                          {to(0U, 13U, "mailto:a@example.com")})},
                  "an e-mail autolink in angle brackets gets mailto:");
    expect_blocks("<https://x.y/a\\_b>",
                  {linked(styled(leaf(paragraph, "https://x.y/a\\_b", 1U), {span(0U, 16U, link)}),
                          {to(0U, 16U, "https://x.y/a\\_b")})},
                  "an autolink keeps its backslashes");
    // "[画像: " は 9 バイト。
    expect_blocks("![i](p.png) [l](q)",
                  {linked(styled(leaf(paragraph, "[\xE7\x94\xBB\xE5\x83\x8F: i] l", 2U),
                                 {span(0U, 11U, image), span(12U, 13U, link)}),
                          {to(12U, 13U, "q")})},
                  "an image destination is not listed");
    expect_blocks("# [h](#x)",
                  {linked(styled(titled(1U, "h", 3U), {span(0U, 1U, link)}), {to(0U, 1U, "#x")})},
                  "a link inside a heading");
    expect_blocks(
        "> [!NOTE]\n> [a](u)",
        {linked(alerted(styled(quoted(leaf(paragraph, "NOTE\na", 2U), 1U),
                               {span(0U, 4U, PreviewStyle::alert_title), span(5U, 6U, link)}),
                        core::PreviewAlert::note),
                {to(5U, 6U, "u")})},
        "a link after an alert marker moves with the spans");
    expect_blocks(
        "~a [b](u)~",
        {linked(styled(leaf(paragraph, "~a b~", 1U), {span(3U, 4U, link)}), {to(3U, 4U, "u")})},
        "a link after a restored single tilde moves with the spans");
}

// リンクの宛先の種類（ADR 0151 の追補 2026-11-19・#1869）。文書へのリンクは画像と同じ
// resolve_local_reference で解く。
[[nodiscard]] bool destined(std::string_view target, const std::optional<core::FilePath> &document,
                            core::LinkKind kind, std::string_view text)
{
    const auto found = core::link_destination(target, document);
    const bool same = found == core::LinkDestination{.kind = kind, .text = std::string(text)};
    if (!same)
    {
        std::fprintf(stderr, "  link_destination([%.*s]) = (%d, [%s])\n",
                     static_cast<int>(target.size()), target.data(), static_cast<int>(found.kind),
                     found.text.c_str());
    }
    return same;
}

[[nodiscard]] bool refused_link(std::string_view target,
                                const std::optional<core::FilePath> &document)
{
    return destined(target, document, core::LinkKind::refused, target);
}

// 見出しの名札（段 L3・#1869）。text の見出しを解析して、名札の列を文書の順に並べた形。
[[nodiscard]] std::vector<std::string>
slugs_of(std::string_view text, core::DisplayLanguage language = core::DisplayLanguage::japanese)
{
    const auto parsed = core::parse_markdown(text, MarkdownStop::never(), language);
    std::vector<std::string> slugs;
    if (!parsed.has_value())
    {
        return slugs;
    }
    for (const auto &each : parsed.value().headings)
    {
        slugs.push_back(each.slug);
    }
    return slugs;
}

[[nodiscard]] bool slugged(std::string_view text, const std::vector<std::string> &expected)
{
    const auto found = slugs_of(text);
    if (found != expected)
    {
        for (const auto &slug : found)
        {
            std::fprintf(stderr, "  slug [%s]\n", slug.c_str());
        }
    }
    return found == expected;
}

// 名札の表。GitHub の公開の説明（Basic writing and formatting syntax の Section links）の規則と例:
// 小文字に・空白は `-`・ほかの空白と記号は消す・装飾は中身だけ・同じ名札は `-1` `-2` …。大小は、
// github-slugger は toLowerCase()（非 ASCII も畳む）で、Nib は ASCII だけを畳む（core に大小の表を
// 持たないため・限界）ので、例の Θ は残る。消す字は github-slugger 2.0.0 の regex.js から作った
// 表の 1 つ（ADR 0151 の段 L3）。
void verify_heading_slugs()
{
    expect(slugged("# Example headings\n\n## Sample Section\n",
                   {"example-headings", "sample-section"}),
           "the examples of the GitHub description: lower case and a hyphen for each space");
    expect(slugged("## This'll be a _Helpful_ Section About the Greek Letter \xCE\x98!\n",
                   {"thisll-be-a-helpful-section-about-the-greek-letter-\xCE\x98"}),
           "markup keeps its contents, ASCII punctuation goes and a non-ASCII letter stays");
    expect(slugged("## This heading is not unique in the file\n\n"
                   "## This heading is not unique in the file\n\n"
                   "## This heading is not unique in the file\n",
                   {"this-heading-is-not-unique-in-the-file",
                    "this-heading-is-not-unique-in-the-file-1",
                    "this-heading-is-not-unique-in-the-file-2"}),
           "a repeated anchor gets a hyphen and an increasing number");
    expect(slugged("# a\n\n# a\n\n# a-1\n", {"a", "a-1", "a-1-1"}),
           "a numbered anchor that already exists moves on to the next number");
    expect(slugged("# C++ & C#\n\n# a  b\n\n# x_y-z 9\n", {"c--c", "a--b", "x_y-z-9"}),
           "symbols go, each space is a hyphen and digits, `-` and `_` stay");
}

// 日本語の文書の名札（設計席の決め 2026-11-19 の追記と、レビューの後の決め）。右の値は
// github-slugger 2.0.0 の slug()（小文字に・regex.js の字を消す・空白を `-` に）の値で、大小を畳む
// のが ASCII だけであることのほかは同じになる。
void verify_heading_slugs_cjk()
{
    expect(slugged("# \xE3\x81\xAF\xE3\x81\x98\xE3\x82\x81\xE3\x81\xAB "
                   "\xE6\xA6\x82\xE8\xA6\x81\n",
                   {"\xE3\x81\xAF\xE3\x81\x98\xE3\x82\x81\xE3\x81\xAB-"
                    "\xE6\xA6\x82\xE8\xA6\x81"}),
           "Japanese letters stay");
    // 「設定（Ctrl+,）」→ 設定ctrl・「手順 1. 準備」→ 手順-1-準備・「🚀 Launch」→ -launch・
    // 「はじめに、概要。」→ はじめに概要・「「引用」と“quote”…」→ 引用とquote・
    // 「❤️ Love」→ U+FE0F-love（❤ U+2764 は記号で消え、VARIATION SELECTOR-16 は結合記号で残る）。
    expect(slugged("# \xE8\xA8\xAD\xE5\xAE\x9A\xEF\xBC\x88"
                   "Ctrl+,\xEF\xBC\x89\n\n"
                   "# \xE6\x89\x8B\xE9\xA0\x86 1. \xE6\xBA\x96\xE5\x82\x99\n\n"
                   "# \xF0\x9F\x9A\x80 Launch\n\n"
                   "# \xE3\x81\xAF\xE3\x81\x98\xE3\x82\x81\xE3\x81\xAB\xE3\x80\x81"
                   "\xE6\xA6\x82\xE8\xA6\x81\xE3\x80\x82\n\n"
                   "# \xE3\x80\x8C\xE5\xBC\x95\xE7\x94\xA8\xE3\x80\x8D\xE3\x81\xA8\xE2\x80\x9C"
                   "quote\xE2\x80\x9D\xE2\x80\xA6\n\n"
                   "# \xE2\x9D\xA4\xEF\xB8\x8F Love\n",
                   {"\xE8\xA8\xAD\xE5\xAE\x9A"
                    "ctrl",
                    "\xE6\x89\x8B\xE9\xA0\x86-1-\xE6\xBA\x96\xE5\x82\x99", "-launch",
                    "\xE3\x81\xAF\xE3\x81\x98\xE3\x82\x81\xE3\x81\xAB\xE6\xA6\x82\xE8\xA6\x81",
                    "\xE5\xBC\x95\xE7\x94\xA8\xE3\x81\xA8quote", "\xEF\xB8\x8F-love"}),
           "punctuation, symbols and emoji go as github-slugger removes them");
    // 文字・数字・結合記号は残る: 「様々な設定」→ 様々な設定（々 は Lm）・「〇〇の件」→ 〇〇の件
    // （〇 は Nl）・「⚠️ 注意」→ U+FE0F-注意・「Ａ＿Ｂ」→ Ａ＿Ｂ（＿ は Pc で `_` と同じく残る）。
    expect(
        slugged("# \xE6\xA7\x98\xE3\x80\x85\xE3\x81\xAA\xE8\xA8\xAD\xE5\xAE\x9A\n\n"
                "# \xE3\x80\x87\xE3\x80\x87\xE3\x81\xAE\xE4\xBB\xB6\n\n"
                "# \xE2\x9A\xA0\xEF\xB8\x8F \xE6\xB3\xA8\xE6\x84\x8F\n\n"
                "# \xEF\xBC\xA1\xEF\xBC\xBF\xEF\xBC\xA2\n",
                {"\xE6\xA7\x98\xE3\x80\x85\xE3\x81\xAA\xE8\xA8\xAD\xE5\xAE\x9A",
                 "\xE3\x80\x87\xE3\x80\x87\xE3\x81\xAE\xE4\xBB\xB6",
                 "\xEF\xB8\x8F-\xE6\xB3\xA8\xE6\x84\x8F", "\xEF\xBC\xA1\xEF\xBC\xBF\xEF\xBC\xA2"}),
        "iteration marks, ideographic zero, the variation selector and full-width low line stay");
    // 句読点と記号は日本語の文書でよく使うものも消える: 「設計・実装」→ 設計実装・「※ 注」→ -注・
    // 「前→後」→ 前後・「① 準備」→ -準備（① は No）・「2×3」→ 23・「90°」→ 90・
    // 「a&nbsp;b」→ ab（NBSP は Zs で、空白 U+0020 だけが `-` になる）。
    expect(slugged("# \xE8\xA8\xAD\xE8\xA8\x88\xE3\x83\xBB\xE5\xAE\x9F\xE8\xA3\x85\n\n"
                   "# \xE2\x80\xBB \xE6\xB3\xA8\n\n"
                   "# \xE5\x89\x8D\xE2\x86\x92\xE5\xBE\x8C\n\n"
                   "# \xE2\x91\xA0 \xE6\xBA\x96\xE5\x82\x99\n\n"
                   "# 2\xC3\x97"
                   "3\n\n"
                   "# 90\xC2\xB0\n\n"
                   "# a&nbsp;b\n",
                   {"\xE8\xA8\xAD\xE8\xA8\x88\xE5\xAE\x9F\xE8\xA3\x85", "-\xE6\xB3\xA8",
                    "\xE5\x89\x8D\xE5\xBE\x8C", "-\xE6\xBA\x96\xE5\x82\x99", "23", "90", "ab"}),
           "the middle dot, reference mark, arrows, circled digits, Latin-1 signs and NBSP go");
    // 非 ASCII の大文字は畳まない（github-slugger は「ＡＢＣ１」を「ａｂｃ１」に畳む・ADR
    // の限界）。
    expect(slugged("# \xEF\xBC\xA1\xEF\xBC\xA2\xEF\xBC\xA3\xEF\xBC\x91\n",
                   {"\xEF\xBC\xA1\xEF\xBC\xA2\xEF\xBC\xA3\xEF\xBC\x91"}),
           "full-width letters and digits stay and are not folded");
    expect(slugged("# **Bold** `code` [link](x)\n\nSetext\n===\n", {"bold-code-link", "setext"}),
           "a decorated heading and a setext heading are named by their shown text");
    expect(slugged("#\n\n#\n\ntext\n", {"", "-1"}), "an empty heading has an empty anchor");
}

// 名札の元は見出しの文字のうち名札に数えるものだけ（レビュー M3・R1）。画像は alt の文字だけで、
// 表示の言語で変わる枠（[画像: …] など）は入らない。生の HTML のタグは入らず、中の文字は残る。
void verify_heading_slug_sources()
{
    constexpr std::string_view imaged = "# ![logo](x.png) Title\n";
    const std::vector<std::string> named{"logo-title"};
    expect(slugs_of(imaged, core::DisplayLanguage::japanese) == named &&
               slugs_of(imaged, core::DisplayLanguage::english) == named &&
               slugs_of(imaged, core::DisplayLanguage::simplified_chinese) == named,
           "an image adds only its alt and the anchor does not change with the display language");
    const auto parsed = core::parse_markdown(imaged, MarkdownStop::never());
    expect(parsed.has_value() && parsed.value().blocks.front().text != "logo Title",
           "the shown text still frames the image");
    expect(slugged("# <img src=\"a.png\" width=\"32\"> Title\n\n"
                   "# <a name=\"x\"></a>Intro\n\n"
                   "# <b>Bold</b> word\n",
                   {"-title", "intro", "bold-word"}),
           "raw HTML tags are not counted and the text between them is");
}

// 名札の列で `#…` を引く。
void verify_heading_lookup()
{
    const auto parsed = core::parse_markdown(
        "intro\n\n# Sample Section\n\n# \xE6\xA6\x82\xE8\xA6\x81\n", MarkdownStop::never());
    expect(parsed.has_value(), "the headings parse");
    const auto &headings = parsed.value().headings;
    expect(headings.size() == 2 && headings.front().block == 1 && headings.back().block == 2,
           "each anchor points at its heading block");
    expect(core::heading_block_of(headings, "sample-section") == std::optional<std::size_t>{1} &&
               core::heading_block_of(headings, "Sample-SECTION") == std::optional<std::size_t>{1},
           "a fragment is matched without ASCII case");
    expect(core::heading_block_of(headings, "%E6%A6%82%E8%A6%81") ==
                   std::optional<std::size_t>{2} &&
               core::heading_block_of(headings, "\xE6\xA6\x82\xE8\xA6\x81") ==
                   std::optional<std::size_t>{2},
           "a fragment is percent-decoded before it is matched");
    expect(!core::heading_block_of(headings, "missing").has_value() &&
               !core::heading_block_of(headings, "sample section").has_value(),
           "an unknown fragment finds nothing");
}

void verify_link_destinations()
{
    using enum core::LinkKind;
    const std::optional<core::FilePath> base = core::FilePath::parse("C:\\docs\\a.md").value();
    const std::optional<core::FilePath> untitled;
    expect(destined("https://x.y/a?b#c", base, web, "https://x.y/a?b#c") &&
               destined("HTTP://X.Y", base, web, "HTTP://X.Y") &&
               destined("MailTo:a@b.c", base, mail, "MailTo:a@b.c"),
           "http, https and mailto are matched without case and keep their spelling");
    expect(refused_link("javascript:alert(1)", base) && refused_link("ms-settings:x", base) &&
               refused_link("data:text/plain,x", base) && refused_link("a.md:stream", base),
           "any other scheme is refused");
    expect(destined("#sec", base, heading, "sec") && destined("#", untitled, heading, ""),
           "a hash first is a heading");
    expect(destined("C:\\x\\b.md", base, document, "C:\\x\\b.md") &&
               destined("c:/x/b.md", base, document, "c:\\x\\b.md"),
           "a one-letter scheme is a drive and an absolute path is a document");
    expect(destined("b.md", base, document, "C:\\docs\\b.md") &&
               destined("../c/./d.md", base, document, "C:\\c\\d.md") &&
               destined("sub/e%20f.md", base, document, "C:\\docs\\sub\\e f.md") &&
               destined("b.md#sec", base, document, "C:\\docs\\b.md") &&
               destined("b.md?q", base, document, "C:\\docs\\b.md") &&
               destined("%zz.md", base, document, "C:\\docs\\%zz.md"),
           "a relative path is resolved from the folder of the document like an image");
    expect(destined("file:///C:/x/y.md", base, document, "C:\\x\\y.md") &&
               refused_link("file://server/s/x.md", base),
           "a file URL is resolved like an image and a host is refused");
    expect(refused_link("b.md", untitled), "a relative path in an untitled document is refused");
    expect(refused_link("\\\\server\\share\\x.md", base) && refused_link("//host/x.md", base),
           "a network path is refused");
    expect(refused_link("%FF.md", base) && refused_link("%00a.md", base),
           "a percent sign that decodes to invalid UTF-8 or NUL is refused");
    expect(refused_link("", base) && refused_link("a\tb.md", base) &&
               refused_link("https://x\x7f", base),
           "an empty destination and control characters are refused");
    expect(refused_link("p.png", base) && refused_link("P.PNG", base) &&
               refused_link("file:///C:/x/y.EXE", base),
           "a destination with an extension that is not text is refused");
}

// OS に渡す web と mail の綴りは ASCII の URI だけ（#1873 のレビュー M1）。
// 非 ASCII の字は path・query・fragment で `%XX` に符号化する。
// host の非 ASCII・URI に無い ASCII・不完全な `%` は断る。
void verify_link_destination_uris()
{
    using enum core::LinkKind;
    const std::optional<core::FilePath> base = core::FilePath::parse("C:\\docs\\a.md").value();
    expect(destined("https://ja.wikipedia.org/wiki/日本?q=語#節", base, web,
                    "https://ja.wikipedia.org/wiki/%E6%97%A5%E6%9C%AC?q=%E8%AA%9E#%E7%AF%80") &&
               destined("https://a/\xE2\x80\xAE"
                        "gpj.exe",
                        base, web, "https://a/%E2%80%AEgpj.exe"),
           "non-ASCII characters after the host are percent-encoded");
    expect(refused_link("https://例え.jp/", base) && refused_link("https://u@例え.jp/", base),
           "a non-ASCII host is refused");
    expect(refused_link("https://a.example/\" --flag", base) && refused_link("https://a b", base) &&
               refused_link("https://a/{x}", base) && refused_link("https://a/x|y", base) &&
               refused_link("https://a/x\\y", base) && refused_link("mailto:a <b@c.d>", base),
           "ASCII characters outside RFC 3986 are refused, not encoded");
    expect(refused_link("https://a/%zz", base) && refused_link("https://a/%4", base) &&
               destined("https://a/%41%e3", base, web, "https://a/%41%e3"),
           "an incomplete percent sign is refused and a complete one is kept");
    expect(refused_link("http:", base) && refused_link("https:x", base) &&
               refused_link("http://", base) && refused_link("http://u@:80/", base) &&
               destined("http://[::1]:80/", base, web, "http://[::1]:80/"),
           "http and https need // and a host");
    expect(refused_link("http://%65vil.com/", base) &&
               refused_link("https://good.example@evil.example/", base),
           "a percent sign or an at sign in the authority is refused");
    expect(destined("mailto:a@b.c?subject=hi&body=x&CC=d@e.f&bcc=g@h.i", base, mail,
                    "mailto:a@b.c?subject=hi&body=x&CC=d@e.f&bcc=g@h.i") &&
               refused_link("mailto:a@b.c?attach=x", base) &&
               refused_link("mailto:a@b.c?%61ttach=x", base) &&
               refused_link("mailto:a@b.c?subject=x&attachment=y", base) &&
               refused_link("mailto:a@b.c?subject=x?attach=y", base),
           "mailto takes only subject, body, cc and bcc");
    expect(refused_link(" https://x.y", base) && refused_link(" b.md", base) &&
               refused_link("b.md ", base),
           "a destination with a leading or trailing space is refused");
}

// 文書へのリンクの経路（#1873 のレビュー M2・M3）。見えない字・不正な UTF-8・
// 装置の予約名・`.` か空白で終わる名前・`\\?\` と `\\.\` は断る。
void verify_link_destination_paths()
{
    using enum core::LinkKind;
    const std::optional<core::FilePath> base = core::FilePath::parse("C:\\docs\\a.md").value();
    expect(refused_link("a\xE2\x80\xAE"
                        "b.md",
                        base) &&
               refused_link("a\xE2\x81\xA6.md", base) && refused_link("a\xE2\x80\x8E.md", base) &&
               refused_link("a\xC2\x85.md", base) && refused_link("a\xE2\x80\x8B.md", base) &&
               refused_link("a\xE2\x81\xA0.md", base) &&
               refused_link("\xEF\xBB\xBF"
                            "a.md",
                            base) &&
               refused_link("a%E2%80%AE.md", base),
           "bidirectional controls, C1 controls and zero-width characters are refused");
    expect(refused_link("a\xD8\x9C.md", base) && refused_link("a\xE1\xA0\x8E.md", base) &&
               refused_link("a\xE1\x85\x9F.md", base) && refused_link("a\xE3\x85\xA4.md", base) &&
               refused_link("a\xEF\xBE\xA0.md", base) && refused_link("a\xF3\xA0\x81\x81.md", base),
           "the Arabic letter mark, Hangul fillers and tag characters are refused");
    expect(refused_link("a\xFF.md", base) && refused_link("a%FF.md", base),
           "invalid UTF-8 is refused");
    expect(refused_link("COM1", base) && refused_link("con.md", base) &&
               refused_link("C:\\x\\AUX", base) && refused_link("NUL.tar.md", base) &&
               refused_link("lpt\xC2\xB9.txt", base) && refused_link("prn", base) &&
               refused_link("CONOUT$", base) && refused_link("file:///C:/x/com0.md", base) &&
               refused_link("./COM1:", base) && refused_link("sub/con:", base) &&
               refused_link("NUL :", base) && refused_link("./NUL:x", base),
           "reserved device names are refused");
    expect(destined("console.md", base, document, "C:\\docs\\console.md") &&
               destined("com10.md", base, document, "C:\\docs\\com10.md") &&
               destined("a.con", base, document, "C:\\docs\\a.con"),
           "names that only look like device names are documents");
    expect(refused_link("x.exe.", base) && refused_link("x.md%20", base) &&
               refused_link("x.md.#sec", base),
           "a name that ends with a dot or a space is refused");
    expect(refused_link("\\\\?\\C:\\x.md", base) && refused_link("\\\\.\\COM1", base) &&
               refused_link("//?/C:/x.md", base),
           "verbatim and device paths are refused");
    expect(destined("file://localhost/C:/x/y.md", base, document, "C:\\x\\y.md") &&
               destined("FILE:/C:/x/y.md", base, document, "C:\\x\\y.md"),
           "a file URL on localhost is a document");
}

void verify_html_and_line_endings()
{
    expect_blocks("<div>\n*hello*\n</div>", {leaf(html, "<div>\n*hello*\n</div>\n", 0U)},
                  "an HTML block keeps its lines as they are");
    expect_blocks("foo <b>bar</b>", {leaf(paragraph, "foo <b>bar</b>", 0U)},
                  "inline HTML stays in the paragraph text");
    expect_blocks("# a\r\n\r\nb\r\nc\r\n", {titled(1U, "a", 2U), leaf(paragraph, "b c", 7U)},
                  "CRLF input gives the same blocks as LF");
    expect_blocks("```\r\nx\r\n```", {leaf(code, "x\n", 5U)},
                  "a CRLF code block ends its lines with LF");
    expect_blocks("", {}, "an empty input has no blocks");
    expect_blocks("\n\n  \n", {}, "blank lines have no blocks");
}

// CR だけの改行も md4c と同じく行の終わりで、位置を持たない要素の行頭はその次の行から探す。
// CRLF は 1 つの改行（#887）。後ろの 2 本は #871 の性質の試験が縮めた入力。
void verify_carriage_returns()
{
    expect_blocks("- foo\r-\r- bar",
                  {listed(leaf(list_item, "foo", 2U), 1U, bullet),
                   listed(leaf(list_item, "", 6U), 1U, bullet),
                   listed(leaf(list_item, "bar", 10U), 1U, bullet)},
                  "an empty item after a CR line ending begins on the next line");
    expect_blocks("Foo\r***\rbar",
                  {leaf(paragraph, "Foo", 0U), leaf(rule, "", 4U), leaf(paragraph, "bar", 8U)},
                  "a thematic break after a CR line ending begins on its own line");
    expect_blocks("a\r\r---", {leaf(paragraph, "a", 0U), leaf(rule, "", 3U)},
                  "a blank line ended by CR is skipped");
    expect_blocks("a\r\n\r\n---", {leaf(paragraph, "a", 0U), leaf(rule, "", 5U)},
                  "CRLF counts as one line ending when skipping blank lines");
    expect_blocks(">[\r+\r\\",
                  {quoted(leaf(paragraph, "[", 1U), 1U),
                   listed(leaf(list_item, "", 3U), 1U, bullet), leaf(paragraph, "\\", 5U)},
                  "an empty item after a quote and a CR begins before the next paragraph");
    expect_blocks("`\r1. 1. \\",
                  {leaf(paragraph, "`", 0U), listed(leaf(list_item, "", 2U), 1U, "1."),
                   listed(leaf(list_item, "\\", 8U), 2U, "1.")},
                  "an empty outer item after a CR begins before its nested item");
}

void verify_stop_and_size()
{
    std::size_t asked = 0;
    const auto second = [&asked]
    {
        ++asked;
        return asked == 2U;
    };
    const auto stopped = core::parse_markdown("# a\n\nb", MarkdownStop::asking(second));
    expect(!stopped.has_value() && stopped.error() == MarkdownFailure::stopped && asked == 2U,
           "a predicate that answers true on the second callback stops the parse");

    // DOC・P・"b " の 3 回目は text のコールバック。md4c の内側の関数は正の値を捨てるので、
    // 負の打ち切りの値が上まで届くこと（#765 の実測）。
    std::size_t texts = 0;
    const auto third = [&texts]
    {
        ++texts;
        return texts == 3U;
    };
    const auto inner = core::parse_markdown("b *c* d", MarkdownStop::asking(third));
    expect(!inner.has_value() && inner.error() == MarkdownFailure::stopped && texts == 3U,
           "a stop on a text callback ends the parse");

    std::size_t spans = 0;
    const auto at_span = [&spans]
    {
        ++spans;
        return spans == 3U;
    };
    const auto span_stop = core::parse_markdown("*a*", MarkdownStop::asking(at_span));
    expect(!span_stop.has_value() && span_stop.error() == MarkdownFailure::stopped && spans == 3U,
           "a stop on enter_span (DOC, P, EM) ends the parse");

    std::size_t leaves = 0;
    const auto at_leave = [&leaves]
    {
        ++leaves;
        return leaves == 4U;
    };
    const auto leave_stop = core::parse_markdown("# a", MarkdownStop::asking(at_leave));
    expect(!leave_stop.has_value() && leave_stop.error() == MarkdownFailure::stopped &&
               leaves == 4U,
           "a stop on leave_block (DOC, H, a, /H) ends the parse");

    std::size_t counted = 0;
    const auto never = [&counted]
    {
        ++counted;
        return false;
    };
    const auto whole = core::parse_markdown("# a\n\nb *c*", MarkdownStop::asking(never));
    expect(whole.has_value() && whole.value().blocks.size() == 2U && counted >= 10U,
           "the predicate is asked on every callback, text included");

    constexpr std::size_t widest = std::numeric_limits<unsigned>::max();
    expect(core::markdown_size_fits(widest) && !core::markdown_size_fits(widest + 1U),
           "the size limit is the md4c size type");
    expect(core::markdown_size_fits(std::size_t{16U} << 20U), "16 MiB fits the md4c size type");
}

// 述語やコールバックが投げた bad_alloc は insufficient_memory で、stopped とは別（#817）。
void verify_insufficient_memory()
{
    const auto exhausted = []() -> bool { throw std::bad_alloc{}; };
    const auto short_of_memory = core::parse_markdown("# a\n\nb", MarkdownStop::asking(exhausted));
    expect(!short_of_memory.has_value() &&
               short_of_memory.error() == MarkdownFailure::insufficient_memory,
           "a bad_alloc thrown inside the parse is insufficient_memory");
    const auto after = core::parse_markdown("# a\n\nb", MarkdownStop::never());
    expect(after.has_value() && after.value().blocks.size() == 2U,
           "a parse after insufficient_memory parses as usual");
    const auto at_once = [] { return true; };
    const auto still_stopped = core::parse_markdown("# a", MarkdownStop::asking(at_once));
    expect(!still_stopped.has_value() && still_stopped.error() == MarkdownFailure::stopped,
           "a predicate that answers true is still stopped, not insufficient_memory");
}
} // namespace

void verify_markdown_scope()
{
    verify_headings();
    verify_paragraphs();
    verify_emphasis();
    verify_inline_code_and_links();
    verify_bullet_lists();
    verify_ordered_lists();
    verify_nested_items();
    verify_tasks();
    verify_quotes();
    verify_code_blocks();
    verify_code_colors();
    verify_code_color_count();
    verify_code_color_places();
    verify_code_pieces();
    verify_code_piece_places();
    verify_code_piece_copy();
    verify_code_piece_motion();
    verify_code_piece_colors();
    verify_rules();
    verify_entities();
    verify_html5_entities();
    verify_strikethrough();
    verify_bare_links();
    verify_front_matter();
    verify_tables();
    verify_alerts();
    verify_link_targets();
    verify_heading_slugs();
    verify_heading_slugs_cjk();
    verify_heading_slug_sources();
    verify_heading_lookup();
    verify_link_destinations();
    verify_link_destination_uris();
    verify_link_destination_paths();
    verify_html_and_line_endings();
    verify_carriage_returns();
    verify_stop_and_size();
    verify_insufficient_memory();
}
} // namespace nenenib::tests
