// scope `--markdown-property` の性質の試験（#871・ADR 0151 / 0155 / 0157・ADR 0042）。
// 外から来るバイト列を受ける 2 つの入口 parse_markdown と markdown_from_html に、固定 seed の
// 決定的な生成器（ランダムなバイト列・構文の断片の列・見本の変異）で作った入力を渡し、
// 落ちない・結果は値か閉じた失敗・上限を守る・同じ入力で同じ結果、を確かめる。
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして
// 回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "HtmlFailure.hpp"
#include "HtmlToMarkdown.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewSpan.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::HtmlFailure;
using core::MarkdownFailure;
using core::MarkdownStop;
using core::PreviewBlock;
using core::PreviewBlockKind;
using Random = std::mt19937_64;
using Parsed = std::expected<core::MarkdownDocument, MarkdownFailure>;
using Converted = std::expected<std::string, HtmlFailure>;
using Make = std::string (*)(Random &);
using Check = std::string_view (*)(std::string_view);

constexpr std::size_t rounds = 1U;
constexpr std::size_t family_inputs = 150U * rounds;
constexpr std::size_t short_input_limit = 512U;
constexpr std::size_t large_input_bytes = std::size_t{64U} * 1024U;
constexpr std::size_t large_inputs = 2U * rounds;
constexpr std::size_t saturated = 255U;
constexpr std::string_view bullet = "•";
constexpr std::string_view hidden = "ZQXJ";

// 50 倍の走りまでに見つかった破れを縮めた回帰 case（#871・直しは別の Issue）。今の答えを固定し、
// 直ったら落ちるので、そのとき表から外して性質の例外（has_lone_carriage_return）も消す。
// 1・2: 位置を持たない要素（空の項目）の行頭を LF だけで探すので、CR だけの改行の後ろの要素が
// 本文の末尾を指し、続く要素より後ろになる。
constexpr std::array<std::string_view, 2> known_breaks{{
    ">[\r+\r\\",
    "`\r1. 1. \\",
}};

// Markdown の構文の断片（見出し・フェンス・箇条書き・引用・表・HTML ブロック・実体参照・
// リンクの参照・バックスラッシュ・改行の形・多バイトと壊れた UTF-8）。
constexpr std::array<std::string_view, 48> markdown_fragments{{
    "# ",
    "###### ",
    "####### ",
    "\n",
    "\n\n",
    "\r\n",
    "\r",
    "\t",
    "    ",
    "```",
    "```c\n",
    "~~~",
    "- ",
    "* ",
    "+ ",
    "1. ",
    "2) ",
    "  - ",
    "> ",
    ">",
    "|a|b|\n",
    "|-|-|\n",
    "<div>",
    "</div>",
    "<!--",
    "-->",
    "<pre>",
    "&amp;",
    "&#x41;",
    "&#0;",
    "&#xD800;",
    "&bogus;",
    "[x]: /u\n",
    "[x]",
    "[x][]",
    "\\",
    "*",
    "**",
    "_",
    "`",
    "![i](s \"t\")",
    "[l](<u>)",
    "<https://a>",
    "---\n",
    "===\n",
    "日本",
    "\xF0\x9F\x98\x80",
    "\xC3",
}};

// 見本（変異の元）。
constexpr std::array<std::string_view, 4> markdown_samples{{
    "# Title\n\nA *b* **c** `d` [e](f) ![g](h)\n\n- one\n  - two\n- three\n\n> q\n> > r\n",
    "1. a\n2. b\n\n```c\nint x;\n```\n\n    code\n\n---\n\n<div>\nhtml\n</div>\n",
    "[ref]: /url \"t\"\n\nSee [ref] and &amp; &copy; &#169; \\* \\_\nline  \nbreak\n",
    "Setext\n===\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n* x\n\n  y\n* z\n",
}};

constexpr std::string_view markdown_alphabet = "#*_`[]()<>!-+=|\\&;:~.\"' \t\n\r0123456789abc";

// HTML の断片（閉じない・属性の引用の崩れ・script と style・実体参照・CDATA・コメント）。
constexpr std::array<std::string_view, 48> html_fragments{{
    "<p>",
    "</p>",
    "<div>",
    "</div>",
    "<b>",
    "</b>",
    "<em>",
    "<span>",
    "</span>",
    "<a href=\"u\">",
    "<a href='u>",
    "<a href=u",
    "</a>",
    "<img src=x alt=\"y\">",
    "<br>",
    "<br/>",
    "<hr>",
    "<ul>",
    "<ol>",
    "<li>",
    "</li>",
    "<pre>",
    "</pre>",
    "<code>",
    "<h1>",
    "<h7>",
    "<blockquote>",
    "<table><tr><td>",
    "</td><td>",
    "<script>x<y</script>",
    "<style>a{}</style>",
    "<script>",
    "&amp;",
    "&#x1F600;",
    "&#0;",
    "&#xD800;",
    "&bogus",
    "<![CDATA[x]]>",
    "<!-- c -->",
    "<!--",
    "<!DOCTYPE html>",
    "<?xml?>",
    "text",
    "日本",
    " \n\t",
    "\r\n",
    "<",
    "</x-y:z>",
}};

constexpr std::array<std::string_view, 3> html_samples{{
    "<h2>A</h2><p>a <b>b</b> <a href=\"https://e.com\">c</a><br>d</p><ul><li>x<ul><li>y</li>"
    "</ul></li></ul>",
    "<blockquote><p>q &amp; &lt;r&gt;</p></blockquote><pre><code>int x;\n</code></pre><hr>"
    "<ol><li>one<li>two</ol>",
    "<table><tr><th>h</th></tr><tr><td>1</td></tr></table><!-- c --><img src=\"i.png\" alt=a>",
}};

constexpr std::string_view html_alphabet = "<>/!-=\"' abc&;#x[]CD?\n\r\t";

// 入れ子にしても暗に閉じない要素（深さの上限の試験）。
constexpr std::array<std::string_view, 6> nesting_tags{
    {"div", "span", "b", "em", "blockquote", "x-custom"}};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] char raw_byte(Random &random)
{
    return static_cast<char>(static_cast<unsigned char>(below(random, 256U)));
}

[[nodiscard]] std::string random_bytes(Random &random, std::string_view alphabet)
{
    const std::size_t length = below(random, short_input_limit);
    const bool raw = below(random, 2U) == 0U;
    std::string bytes;
    bytes.reserve(length);
    for (std::size_t index = 0; index < length; ++index)
    {
        const bool pick_raw = raw && below(random, 4U) == 0U;
        bytes.push_back(pick_raw ? raw_byte(random) : alphabet[below(random, alphabet.size())]);
    }
    return bytes;
}

template <std::size_t Count>
[[nodiscard]] std::string_view pick(Random &random, const std::array<std::string_view, Count> &from)
{
    return from.at(below(random, Count));
}

// 断片を並べる。ときどき 1 つを深く繰り返す（引用・箇条書き・要素の深い入れ子）。
template <std::size_t Count>
[[nodiscard]] std::string composed(Random &random, const std::array<std::string_view, Count> &from)
{
    const std::size_t pieces = 1U + below(random, 64U);
    std::string text;
    for (std::size_t piece = 0; piece < pieces; ++piece)
    {
        const std::string_view fragment = pick(random, from);
        const std::size_t repeat = below(random, 8U) == 0U ? 1U + below(random, 300U) : 1U;
        for (std::size_t copy = 0; copy < repeat; ++copy)
        {
            text.append(fragment);
        }
    }
    return text;
}

// 見本の変異（置換・削除・挿入・切り詰め）。
template <std::size_t Samples, std::size_t Fragments>
[[nodiscard]] std::string mutated(Random &random,
                                  const std::array<std::string_view, Samples> &samples,
                                  const std::array<std::string_view, Fragments> &fragments)
{
    std::string text(pick(random, samples));
    const std::size_t edits = 1U + below(random, 8U);
    for (std::size_t edit = 0; edit < edits && !text.empty(); ++edit)
    {
        const std::size_t at = below(random, text.size());
        const std::size_t operation = below(random, 4U);
        if (operation == 0U)
        {
            text[at] = raw_byte(random);
        }
        else if (operation == 1U)
        {
            text.erase(at, 1U + below(random, 16U));
        }
        else if (operation == 2U)
        {
            text.insert(at, pick(random, fragments));
        }
        else
        {
            text.resize(at);
        }
    }
    return text;
}

// 大きな入力（64 KiB）は断片の列を大きさまで重ねる。
template <std::size_t Count>
[[nodiscard]] std::string large(Random &random, const std::array<std::string_view, Count> &from)
{
    std::string text;
    text.reserve(large_input_bytes + 4096U);
    while (text.size() < large_input_bytes)
    {
        text.append(composed(random, from));
    }
    text.resize(large_input_bytes);
    return text;
}

std::string markdown_bytes(Random &random)
{
    return random_bytes(random, markdown_alphabet);
}
std::string markdown_composed(Random &random)
{
    return composed(random, markdown_fragments);
}
std::string markdown_mutated(Random &random)
{
    return mutated(random, markdown_samples, markdown_fragments);
}
std::string markdown_large(Random &random)
{
    return large(random, markdown_fragments);
}
std::string html_bytes(Random &random)
{
    return random_bytes(random, html_alphabet);
}
std::string html_composed(Random &random)
{
    return composed(random, html_fragments);
}
std::string html_mutated(Random &random)
{
    return mutated(random, html_samples, html_fragments);
}
std::string html_large(Random &random)
{
    return large(random, html_fragments);
}

// 破れた入力を 16 進で出す（160 バイトまで）。
void print_bytes(std::string_view input)
{
    const std::size_t shown = std::min<std::size_t>(input.size(), 160U);
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

// 大きさ chunk の 1 か所を消して、同じ理由で破れたままなら残す。
[[nodiscard]] bool shrunk(std::string &input, std::size_t chunk, Check check,
                          std::string_view reason)
{
    for (std::size_t at = 0; at + chunk <= input.size(); at += chunk)
    {
        std::string candidate = input;
        candidate.erase(at, chunk);
        if (check(candidate) == reason)
        {
            input = std::move(candidate);
            return true;
        }
    }
    return false;
}

// 同じ理由で破れる小さな入力へ貪欲に縮める（回帰 case の手がかり）。
[[nodiscard]] std::string minimized(std::string input, Check check)
{
    const std::string_view reason = check(input);
    std::size_t chunk = std::max<std::size_t>(input.size() / 2U, 1U);
    while (chunk > 0U)
    {
        chunk = shrunk(input, chunk, check, reason) ? std::min(chunk, input.size()) : chunk / 2U;
    }
    return input;
}

void report(const char *family, const std::string &input, Check check)
{
    const std::string_view reason = check(input);
    const std::string smallest = minimized(input, check);
    std::fprintf(stderr, "  %s: %.*s (%zu bytes, minimized to %zu):", family,
                 static_cast<int>(reason.size()), reason.data(), input.size(), smallest.size());
    print_bytes(smallest);
}

// 生成した入力のうち性質を破ったものの数。最初の 1 つは縮めて 16 進で出す。
[[nodiscard]] std::size_t broken_inputs(const char *description, std::uint64_t seed, Make make,
                                        Check check)
{
    Random random{seed};
    const std::size_t count =
        make == markdown_large || make == html_large ? large_inputs : family_inputs;
    std::size_t broken = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string input = make(random);
        if (!check(input).empty() && broken++ == 0U)
        {
            report(description, input, check);
        }
    }
    return broken;
}

void verify_family(const char *description, std::uint64_t seed, Make make, Check check)
{
    expect(broken_inputs(description, seed, make, check) == 0U, description);
}

// --- parse_markdown の不変量（MarkdownDocument.hpp / PreviewBlock.hpp から読める性質）---

[[nodiscard]] bool marker_holds(std::string_view marker)
{
    if (marker.empty() || marker == bullet)
    {
        return true;
    }
    const std::string_view digits = marker.substr(0U, marker.size() - 1U);
    return marker.ends_with('.') && !digits.empty() &&
           std::ranges::all_of(digits,
                               [](char character) { return character >= '0' && character <= '9'; });
}

// 装飾は要素の text の内の半開区間で、開いた順（begin の順）に並ぶ。
[[nodiscard]] bool spans_hold(const PreviewBlock &block)
{
    std::size_t previous = 0U;
    for (const auto &span : block.spans)
    {
        if (span.begin.value < previous || span.end.value < span.begin.value ||
            span.end.value > block.text.size())
        {
            return false;
        }
        previous = span.begin.value;
    }
    return true;
}

// 種類と段と深さの組。破れた性質の名前で、守られていれば空。
[[nodiscard]] std::string_view shape_violation(const PreviewBlock &block)
{
    const bool titled = block.kind == PreviewBlockKind::heading;
    const bool listed = block.kind == PreviewBlockKind::list_item || !block.marker.empty();
    if (titled ? block.level < 1U || block.level > 6U : block.level != 0U)
    {
        return "level outside 1..6 for a heading or not 0 otherwise";
    }
    if (listed && block.list_depth == 0U)
    {
        return "a list item or marker outside any list";
    }
    const bool bare = block.text.empty() && block.spans.empty();
    return block.kind == PreviewBlockKind::rule && !bare ? "a rule with text" : std::string_view{};
}

[[nodiscard]] std::string_view block_violation(std::string_view input, const PreviewBlock &block)
{
    if (block.source_begin.value > input.size())
    {
        return "source_begin past the end of the text";
    }
    const std::string_view shape = shape_violation(block);
    if (!shape.empty())
    {
        return shape;
    }
    if (!marker_holds(block.marker))
    {
        return "marker is not empty, the bullet or digits and a dot";
    }
    return spans_hold(block) ? std::string_view{} : "a span outside the text or out of order";
}

// 後ろに LF の無い CR（CommonMark では行の終わり）。位置を持たない要素の行頭の探索は LF だけを
// 行の終わりとみなすので、これを含む本文では順の性質を問わない（known_breaks の 1・2・#871）。
[[nodiscard]] bool has_lone_carriage_return(std::string_view input)
{
    for (std::size_t at = input.find('\r'); at != std::string_view::npos;
         at = input.find('\r', at + 1U))
    {
        if (at + 1U == input.size() || input[at + 1U] != '\n')
        {
            return true;
        }
    }
    return false;
}

// 要素は本文の順に並ぶ（source_begin は減らない）。
[[nodiscard]] std::string_view document_violation(std::string_view input,
                                                  const core::MarkdownDocument &document)
{
    std::size_t previous = 0U;
    for (const auto &block : document.blocks)
    {
        const std::string_view broken = block_violation(input, block);
        if (!broken.empty())
        {
            return broken;
        }
        if (block.source_begin.value < previous && !has_lone_carriage_return(input))
        {
            return "blocks out of source order";
        }
        previous = block.source_begin.value;
    }
    return {};
}

// 打ち切らない述語では文書が返る・不変量・別の場所に写した同じ入力で同じ結果。
[[nodiscard]] std::string_view markdown_violation(std::string_view input)
{
    const Parsed first = core::parse_markdown(input, MarkdownStop::never());
    const std::string copy(input);
    const Parsed second = core::parse_markdown(copy, MarkdownStop::never());
    if (!first.has_value())
    {
        return "no document without a stop";
    }
    if (!second.has_value() || first.value().blocks != second.value().blocks)
    {
        return "a copy of the same text parses differently";
    }
    return document_violation(input, first.value());
}

// 途中で打ち切る述語は stopped か、打ち切らなかったときと同じ文書。
[[nodiscard]] std::string_view stop_violation(std::string_view input)
{
    std::size_t asked = 0U;
    const std::size_t limit = input.size() % 97U;
    const auto ask = [&asked, limit] { return ++asked > limit; };
    const Parsed stopped = core::parse_markdown(input, MarkdownStop::asking(ask));
    if (!stopped.has_value())
    {
        return stopped.error() == MarkdownFailure::stopped ? std::string_view{}
                                                           : "a stop that is not stopped";
    }
    const Parsed whole = core::parse_markdown(input, MarkdownStop::never());
    return whole.has_value() && whole.value().blocks == stopped.value().blocks
               ? std::string_view{}
               : "an unstopped parse differs from the whole document";
}

[[nodiscard]] std::string repeated(std::string_view piece, std::size_t count)
{
    std::string text;
    for (std::size_t copy = 0; copy < count; ++copy)
    {
        text.append(piece);
    }
    return text;
}

// 引用と箇条書きの深さは 255 で飽和する（PreviewBlock の深さは数で持つ）。
void verify_markdown_depths()
{
    constexpr std::array<std::size_t, 7> depths{{1U, 2U, 64U, 254U, 255U, 256U, 300U}};
    bool quotes = true;
    bool lists = true;
    for (const std::size_t depth : depths)
    {
        const auto quoted =
            core::parse_markdown(repeated("> ", depth) + "x", MarkdownStop::never());
        quotes = quotes && quoted.has_value() && quoted.value().blocks.size() == 1U &&
                 quoted.value().blocks.front().quote_depth == std::min(depth, saturated);
        const auto listed =
            core::parse_markdown(repeated("- ", depth) + "x", MarkdownStop::never());
        lists = lists && listed.has_value() && !listed.value().blocks.empty() &&
                listed.value().blocks.back().list_depth == std::min(depth, saturated);
    }
    expect(quotes, "nested quotes saturate the quote depth at 255");
    expect(lists, "nested list items saturate the list depth at 255");
}

// md4c の大きさの型（unsigned）の境目。上限ちょうどは入り、上限 + 1 は断る。
void verify_markdown_size_boundary()
{
    constexpr std::size_t widest = std::numeric_limits<unsigned>::max();
    expect(core::markdown_size_fits(widest) && !core::markdown_size_fits(widest + 1U) &&
               core::markdown_size_fits(core::preview_byte_limit) && core::markdown_size_fits(0U),
           "markdown_size_fits takes the md4c size type exactly and refuses one byte more");
}

[[nodiscard]] std::string_view order_violation(std::string_view input)
{
    const Parsed parsed = core::parse_markdown(input, MarkdownStop::never());
    if (!parsed.has_value())
    {
        return "no document without a stop";
    }
    std::size_t previous = 0U;
    for (const auto &block : parsed.value().blocks)
    {
        if (block.source_begin.value < previous)
        {
            return "blocks out of source order";
        }
        previous = block.source_begin.value;
    }
    return {};
}

void verify_known_breaks()
{
    bool still = true;
    for (const std::string_view input : known_breaks)
    {
        still = still && order_violation(input) == "blocks out of source order" &&
                markdown_violation(input).empty();
    }
    expect(still, "the known breaks still put an unlocated block after its successor (#871)");
}

void verify_markdown_properties()
{
    verify_known_breaks();
    verify_family("parse_markdown holds on random bytes", 8711U, markdown_bytes,
                  markdown_violation);
    verify_family("parse_markdown holds on composed fragments", 8712U, markdown_composed,
                  markdown_violation);
    verify_family("parse_markdown holds on mutated samples", 8713U, markdown_mutated,
                  markdown_violation);
    verify_family("parse_markdown holds on 64 KiB of fragments", 8714U, markdown_large,
                  markdown_violation);
    verify_family("a stopping predicate is stopped or the whole document", 8715U, markdown_composed,
                  stop_violation);
    verify_markdown_depths();
    verify_markdown_size_boundary();
}

// --- markdown_from_html の不変量 ---

// 出力は上限以内・正しい UTF-8・改行は LF だけ。
[[nodiscard]] std::string_view output_violation(std::string_view markdown)
{
    if (markdown.size() > core::html_markdown_limit)
    {
        return "output over the Markdown limit";
    }
    if (!core::validate_utf8(markdown).has_value())
    {
        return "output is not UTF-8";
    }
    return markdown.contains('\r') ? "output has a carriage return" : std::string_view{};
}

// 同じ入力で同じ結果・正しくない UTF-8 は malformed・小さな入力の失敗は malformed。
[[nodiscard]] std::string_view html_violation(std::string_view input)
{
    const Converted first = core::markdown_from_html(input);
    const std::string copy(input);
    if (first != core::markdown_from_html(copy))
    {
        return "a copy of the same fragment converts differently";
    }
    if (!first.has_value())
    {
        return first.error() == HtmlFailure::malformed ? std::string_view{}
                                                       : "a small fragment is not malformed";
    }
    if (!core::validate_utf8(input).has_value())
    {
        return "a fragment that is not UTF-8 converts";
    }
    return output_violation(first.value());
}

[[nodiscard]] std::string nested_html(Random &random, std::size_t depth)
{
    std::string text;
    for (std::size_t level = 0; level < depth; ++level)
    {
        text.append("<").append(pick(random, nesting_tags)).append(">");
        text.append(below(random, 2U) == 0U ? "t" : "");
    }
    return text + "x";
}

// 入れ子は 64 まで値、65 以上は malformed（閉じない要素は fragment の終わりで閉じる）。
void verify_html_depths()
{
    Random random{8721U};
    bool holds = true;
    for (std::size_t round = 0; round < family_inputs; ++round)
    {
        const std::size_t depth = 1U + below(random, 200U);
        const Converted converted = core::markdown_from_html(nested_html(random, depth));
        const bool refused = !converted.has_value() && converted.error() == HtmlFailure::malformed;
        holds = holds && (depth <= core::html_depth_limit ? converted.has_value() : refused);
    }
    expect(holds, "nesting up to 64 converts and nesting beyond 64 is malformed");
    expect(core::markdown_from_html(repeated("<div>", 64U)).has_value() &&
               core::markdown_from_html(repeated("<div>", 65U)) ==
                   std::unexpected(HtmlFailure::malformed),
           "64 open divs convert and 65 are malformed");
}

// script・style・コメントの中に目印を隠した fragment（前に見本・後ろに段落）。前に崩れた断片
// （`<a href=u` など）を置くと、続く `<script>` が属性に食われて中身が文字になるので見本を使う。
std::string html_hiding(Random &random)
{
    constexpr std::array<std::string_view, 3> openings{{"<script>", "<style>", "<!--"}};
    constexpr std::array<std::string_view, 3> closings{{"</script>", "</style>", "-->"}};
    const std::size_t which = below(random, openings.size());
    std::string html(pick(random, html_samples));
    html.append(openings.at(which)).append(random_bytes(random, "abc <>&;\"'=\n"));
    html.append(hidden).append(closings.at(which)).append("<p>after</p>");
    return html;
}

// 隠した目印は Markdown に出ない。
[[nodiscard]] std::string_view hidden_violation(std::string_view input)
{
    const Converted converted = core::markdown_from_html(input);
    return converted.has_value() && converted.value().contains(hidden)
               ? "the contents of script, style or a comment reached the Markdown"
               : std::string_view{};
}

// 上限 + 1 の fragment は中を読まずに too_large。
void verify_html_size_boundary()
{
    const std::string over(core::html_fragment_limit + 1U, 'a');
    expect(core::markdown_from_html(over) == std::unexpected(HtmlFailure::too_large),
           "a fragment one byte over 16 MiB is too_large");
}

void verify_html_properties()
{
    verify_family("markdown_from_html holds on random bytes", 8731U, html_bytes, html_violation);
    verify_family("markdown_from_html holds on composed fragments", 8732U, html_composed,
                  html_violation);
    verify_family("markdown_from_html holds on mutated samples", 8733U, html_mutated,
                  html_violation);
    verify_family("markdown_from_html holds on 64 KiB of fragments", 8734U, html_large,
                  html_violation);
    verify_html_depths();
    verify_family("the contents of script, style and comments never reach the Markdown", 8722U,
                  html_hiding, hidden_violation);
    verify_html_size_boundary();
}

// 生成器そのものが決定的（同じ seed で同じ列）。
void verify_generators_are_deterministic()
{
    Random left{8741U};
    Random right{8741U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && markdown_composed(left) == markdown_composed(right) &&
               html_mutated(left) == html_mutated(right);
    }
    expect(same, "the generators give the same inputs for the same seed");
}
} // namespace

void verify_markdown_property_scope()
{
    verify_generators_are_deterministic();
    verify_markdown_properties();
    verify_html_properties();
}
} // namespace nenenib::tests
