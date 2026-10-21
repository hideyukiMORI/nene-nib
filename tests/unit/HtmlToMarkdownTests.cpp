// scope `--html-markdown` の単体テスト（ADR 0157 の決定 5〜7・Issue #774）。
// クリップボードの HTML の fragment を Markdown に写す core の純関数 markdown_from_html と、
// 属性・実体参照の読み。対応する要素の記法・空白の畳み込み・知らない要素・閉じない要素・
// 壊れた HTML・空・上限を表で確かめる。
#include "HtmlAttribute.hpp"
#include "HtmlFailure.hpp"
#include "HtmlToMarkdown.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using core::HtmlFailure;
using core::markdown_from_html;

// fragment → 出る Markdown（LF）。
using Conversion = std::array<std::string_view, 3>;

constexpr std::array<Conversion, 76> conversions{{
    // 見出し
    {"<h1>Title</h1>", "# Title", "h1 is one hash"},
    {"<h3>Sub</h3>", "### Sub", "h3 is three hashes"},
    {"<h6>x</h6>", "###### x", "h6 is six hashes"},
    {"<h2>A</h2><p>b</p>", "## A\n\nb", "a heading is followed by a blank line"},
    {"<h2>a <em>b</em></h2>", "## a *b*", "a heading keeps its inline marks"},
    {"<h1></h1><p>x</p>", "x", "an empty heading leaves nothing"},
    // 段落と br
    {"<p>one</p><p>two</p>", "one\n\ntwo", "paragraphs are separated by a blank line"},
    {"<p>a<br>b</p>", "a  \nb", "br is two trailing spaces and a newline"},
    {"<p>a<br/>b</p>", "a  \nb", "a self-closed br is a line break"},
    {"line1<br>line2<br>", "line1  \nline2", "a trailing br leaves no trailing spaces"},
    {"<br>x", "x", "a leading br leaves nothing"},
    // 強調
    {"<strong>bold</strong>", "**bold**", "strong is two asterisks"},
    {"<b>bold</b> and <i>it</i>", "**bold** and *it*", "b and i are strong and emphasis"},
    {"<em>e</em>", "*e*", "em is one asterisk"},
    {"a<b> b </b>c", "a **b** c", "spaces inside emphasis move outside the marks"},
    {"<b></b>x", "x", "an empty strong leaves no marks"},
    {"<b><a href=u>x</a></b>", "**[x](u)**", "a link inside strong keeps both"},
    // リンク
    {"<a href=\"https://e.com\">link</a>", "[link](https://e.com)", "a link is [text](href)"},
    {"<a href='x?a=1&amp;b=2'>q</a>", "[q](x?a=1&b=2)", "href entities are decoded"},
    {"<a name=\"n\">plain</a>", "plain", "an anchor without href is its text"},
    {"x<a href=u></a>y", "xy", "an empty link leaves nothing"},
    {"<a href=\"x>y\">t</a>", "[t](x>y)", "a quoted > does not end the tag"},
    // リスト
    {"<ul><li>a</li><li>b</li></ul>", "- a\n- b", "ul items are dashes"},
    {"<ol><li>a</li><li>b</li><li>c</li></ol>", "1. a\n2. b\n3. c", "ol items are numbered"},
    {"<ul><li>a<ul><li>b</li></ul></li><li>c</li></ul>", "- a\n  - b\n- c",
     "a nested list is indented by two spaces"},
    {"<ul><li>a<li>b</ul>", "- a\n- b", "an unclosed li ends at the next li"},
    {"<ul><li>a<ol><li>x</li><li>y</li></ol></li></ul>", "- a\n  1. x\n  2. y",
     "an ordered list nests in a bullet list"},
    {"<p>p</p><ul><li>a</li></ul><p>q</p>", "p\n\n- a\n\nq",
     "a list is separated from paragraphs by blank lines"},
    {"<ol><li><p>a</p></li><li><p>b</p></li></ol>", "1. a\n\n2. b",
     "paragraphs in items keep the item marker"},
    // 引用
    {"<blockquote>q</blockquote>", "> q", "a quote line starts with >"},
    {"<blockquote><p>a</p><p>b</p></blockquote>", "> a\n>\n> b",
     "paragraphs in a quote are separated by a quoted blank line"},
    {"<p>x</p><blockquote>q</blockquote><p>y</p>", "x\n\n> q\n\ny",
     "a quote is separated by unquoted blank lines"},
    {"<blockquote>a<blockquote>b</blockquote></blockquote>", "> a\n>\n> > b",
     "a nested quote doubles the marks"},
    {"<blockquote>a<br>b</blockquote>", "> a  \n> b", "a line break in a quote keeps the mark"},
    // コード
    {"<code>x</code>", "`x`", "inline code is backticks"},
    {"use <code>ls -l</code> now", "use `ls -l` now", "inline code sits in the sentence"},
    {"<pre>line1\nline2</pre>", "```\nline1\nline2\n```", "pre is a fenced block"},
    {"<pre><code class=\"language-cpp\">int x;\n</code></pre>", "```cpp\nint x;\n```",
     "pre > code language-xxx names the fence"},
    {"<pre><code class=\"hl language-py\">x</code></pre>", "```py\nx\n```",
     "the language is one class among others"},
    {"<pre>\nfirst</pre>", "```\nfirst\n```", "the newline right after pre is dropped"},
    {"<pre>  a  b</pre>", "```\n  a  b\n```", "spaces in pre are kept"},
    {"<pre>a &lt;b&gt;</pre>", "```\na <b>\n```", "entities in pre are decoded"},
    {"<pre><b>x</b></pre>", "```\nx\n```", "emphasis in pre writes no marks"},
    {"<pre>a\r\nb</pre>", "```\na\nb\n```", "CRLF in pre becomes LF"},
    {"<p>a</p><pre>c</pre><p>b</p>", "a\n\n```\nc\n```\n\nb",
     "a fenced block is separated by blank lines"},
    // 水平線と画像
    {"<hr>", "---", "hr is three dashes"},
    {"<p>a</p><hr><p>b</p>", "a\n\n---\n\nb", "hr stands between blank lines"},
    {"<img src=\"p.png\" alt=\"pic\">", "![pic](p.png)", "img is ![alt](src)"},
    {"<img src=p.png>", "![](p.png)", "img without alt has empty brackets"},
    {"a<img alt=x>b", "ab", "img without src leaves nothing"},
    // 表（記法は後続・セルを空白で区切る）
    {"<table><tr><th>A</th><th>B</th></tr><tr><td>1</td><td>2</td></tr></table>", "A B\n1 2",
     "table cells are separated by spaces, rows by newlines"},
    {"<p>x</p><table><tr><td>1</td></tr></table><p>y</p>", "x\n\n1\n\ny",
     "a table is separated by blank lines"},
    // script と style
    {"a<script>var x = '<b>';</script>b", "ab", "script content is dropped"},
    {"<style>p { color: red }</style><p>t</p>", "t", "style content is dropped"},
    {"<SCRIPT>x</SCRIPT>y", "y", "upper-case script is dropped"},
    {"a<script>never closed", "a", "an unclosed script drops the rest"},
    // 実体参照
    {"&amp; &lt; &gt; &quot; &#39;", "& < > \" '", "the common entities are decoded"},
    {"&#65;&#x42;", "AB", "numeric entities are decoded"},
    {"a&nbsp;&nbsp;b", "a  b", "nbsp is a space that is not folded"},
    {"&unknown; & x", "&unknown; & x", "unknown entities stay as written"},
    // 空白の畳み込み
    {"  a \n\t b  ", "a b", "whitespace runs fold to one space and the ends are trimmed"},
    {"<p>\n  hello\n  world\n</p>", "hello world", "newlines in a paragraph fold to spaces"},
    // 知らない要素・div・span
    {"<span>s</span><custom-el>c</custom-el>", "sc", "unknown elements are their text"},
    {"<div>a</div><div>b</div>", "a\nb", "div is a line of its own without marks"},
    {"<font color=red>r</font>", "r", "font is its text"},
    {"<meta charset=utf-8><input value=x>t", "t", "void elements leave nothing"},
    // 閉じない要素・壊れた閉じ
    {"<b>bold", "**bold**", "an unclosed strong is closed at the end"},
    {"<ul><li>a", "- a", "an unclosed list is closed at the end"},
    {"<p><b>x</p>y", "**x**\n\ny", "closing a paragraph closes the strong inside"},
    {"<pre>code", "```\ncode\n```", "an unclosed pre is fenced at the end"},
    {"a</b>b", "ab", "a stray end tag is ignored"},
    // 注釈・宣言・文字の <
    {"a<!-- c -->b", "ab", "comments are dropped"},
    {"<!DOCTYPE html><p>x</p>", "x", "the doctype is dropped"},
    {"a < b", "a < b", "a lone < is text"},
    {"<!--StartFragment--><b>x</b><!--EndFragment-->", "**x**",
     "the fragment comments of CF_HTML are dropped"},
    {"<P>A</P><STRONG>B</STRONG>", "A\n\n**B**", "tag names are case-insensitive"},
}};

void verify_conversions()
{
    for (const auto &[html, markdown, description] : conversions)
    {
        const auto converted = markdown_from_html(html);
        const bool same = converted.has_value() && converted.value() == markdown;
        if (!same)
        {
            std::fprintf(stderr, "html: [%.*s] -> [%s]\n", static_cast<int>(html.size()),
                         html.data(), converted.value_or("<failure>").c_str());
        }
        expect(same, description.data());
    }
}

void verify_japanese_and_empty()
{
    expect(markdown_from_html("<p>日本語の<b>太字</b></p>") == "日本語の**太字**",
           "UTF-8 text passes through");
    expect(markdown_from_html("") == "", "an empty fragment is empty Markdown");
    expect(markdown_from_html("<p></p><div></div>") == "", "empty blocks are empty Markdown");
}

[[nodiscard]] bool fails_with(std::string_view html, HtmlFailure failure)
{
    const auto converted = markdown_from_html(html);
    return !converted.has_value() && converted.error() == failure;
}

void verify_failures()
{
    expect(fails_with("<b", HtmlFailure::malformed), "an unfinished start tag is malformed");
    expect(fails_with("a</b", HtmlFailure::malformed), "an unfinished end tag is malformed");
    expect(fails_with("a<!-- c", HtmlFailure::malformed), "an unfinished comment is malformed");
    expect(fails_with("<!DOCTYPE html", HtmlFailure::malformed),
           "an unfinished declaration is malformed");
    expect(fails_with("<a href=\"x>t</a>", HtmlFailure::malformed),
           "an unclosed quote in a tag is malformed");
    expect(fails_with("a\xFF", HtmlFailure::malformed), "invalid UTF-8 is malformed");
}

void verify_limit()
{
    const std::string at_limit(core::html_fragment_limit, 'a');
    const auto converted = markdown_from_html(at_limit);
    expect(converted.has_value() && converted.value().size() == core::html_fragment_limit,
           "a fragment at the limit is converted");
    const std::string over_limit(core::html_fragment_limit + 1U, 'a');
    expect(fails_with(over_limit, HtmlFailure::too_large), "a fragment over the limit is refused");
}

void verify_attributes()
{
    using core::html_attribute;
    expect(html_attribute(" href=\"a\" class=b", "href") == std::optional<std::string>{"a"},
           "a double-quoted value is read");
    expect(html_attribute(" href=\"a\" class=b", "class") == std::optional<std::string>{"b"},
           "an unquoted value is read");
    expect(html_attribute(" HREF='q'", "href") == std::optional<std::string>{"q"},
           "attribute names are case-insensitive");
    expect(html_attribute(" disabled", "disabled") == std::optional<std::string>{""},
           "an attribute without a value is empty");
    expect(!html_attribute(" a=1", "b").has_value(), "a missing attribute has no value");
    expect(html_attribute(" title = 'x y' src=z", "src") == std::optional<std::string>{"z"},
           "spaces around = are allowed");
    expect(core::html_decoded("a&amp;b&#x41;") == "a&bA", "entities in text are decoded");
    expect(core::html_decoded("a&b") == "a&b", "a lone ampersand stays");
}
} // namespace

void verify_html_markdown_scope()
{
    verify_conversions();
    verify_japanese_and_empty();
    verify_failures();
    verify_limit();
    verify_attributes();
}
} // namespace nenenib::tests
