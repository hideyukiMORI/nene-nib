// parse_markdown（ADR 0151 の決定 5〜8・#765）。入力は CommonMark 0.31 の例を 8 つの構文
// （見出し・段落・強調・リンク・リスト・引用・コード・水平線）と実体参照・改行の範囲で写したもの。
// 期待は CommonMark の HTML を平らな leaf の列に読み替えた値。source_begin は最初の文字の位置。
#include "MarkdownDocument.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "MarkdownTask.hpp"
#include "Offset.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewSpan.hpp"
#include "PreviewStyle.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <new>
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
                        .task = std::nullopt};
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

[[nodiscard]] PreviewSpan span(std::size_t begin, std::size_t end, PreviewStyle style)
{
    return PreviewSpan{Offset{begin}, Offset{end}, style};
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
    expect_blocks("[link](/uri \"title\")",
                  {styled(leaf(paragraph, "link", 1U), {span(0U, 4U, link)})},
                  "an inline link keeps only its text");
    expect_blocks("<https://foo.bar.baz>",
                  {styled(leaf(paragraph, "https://foo.bar.baz", 1U), {span(0U, 19U, link)})},
                  "an autolink shows its address");
    expect_blocks("[*a*](u)",
                  {styled(leaf(paragraph, "a", 2U), {span(0U, 1U, link), span(0U, 1U, emphasis)})},
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
    expect_blocks("3) a", {listed(leaf(list_item, "a", 3U), 1U, "3.")},
                  "a parenthesis delimiter is shown with a period");
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
    verify_rules();
    verify_entities();
    verify_html_and_line_endings();
    verify_carriage_returns();
    verify_stop_and_size();
    verify_insufficient_memory();
}
} // namespace nenenib::tests
