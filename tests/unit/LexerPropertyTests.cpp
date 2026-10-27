// scope `--lexer-property` の単体テスト（#868・ADR 0156 の決定 1〜6・14）。字句器を持つ全言語に
// 想定外の入力（不正な UTF-8・NUL・CR・Tab・区切りの断片・fixture の見本の行の変異・長い行）を
// 文書のように続けて通し、次の不変量を守る。塗りの中身は問わない（それは `--lexer` の fixture）。
//   1. 区間は begin < end・begin の順・重ならない・行の桁の数（`lenient_code_point_count`・
//      不正な UTF-8 のバイトは 1 つで 1 桁・#878）を超えない・役割は閉じた enum の値で warning を
//      出さない。不正な UTF-8 の行でも begin < end を求める。
//   2. lex_state(言語, in, 行) == lex_line(言語, in, 行).out。
//   3. 同じ入力を 2 回通して同じ結果。
//   4. 状態は字句器自身が出した値だけを次の行へ渡し（ランダムな状態は作らない）、どの状態も
//      種類が閉じた enum の値で delimiter の長さが raw_delimiter_limit 以内。
// 生成器は固定 seed の std::mt19937_64 で、分布の型は使わず剰余で範囲に畳む。時刻と環境は
// 読まない（ARC-007）。回数は Debug（ASan / UBSan）で 1 秒以内に収まるように決めた
// （property_scale を 50 にした走りを一度だけ手元で回し、commit しない）。
#include "ColorSpan.hpp"
#include "Column.hpp"
#include "Language.hpp"
#include "LexKind.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"
#include "Lexer.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::ColorSpan;
using core::Language;
using core::LexKind;
using core::LexLine;
using core::LexState;
using core::SyntaxRole;
using Random = std::mt19937_64;
using Lines = std::span<const std::string_view>;

// 回数の倍率。手元の 50 倍の走りだけで変え、commit は 1 のまま。
constexpr std::size_t property_scale = 1U;
constexpr std::uint64_t property_seed = 0x0868'1e8e'5eedULL;
constexpr std::size_t random_lines = 24U * property_scale;
constexpr std::size_t fragment_lines = 48U * property_scale;
constexpr std::size_t mutated_rounds = 6U * property_scale;
constexpr std::size_t long_lines = property_scale;
constexpr std::size_t short_long_line = 4096U;
constexpr std::size_t long_long_line = 65536U;
// 破れを書き出す上限（1 つの破れが全言語・全行に広がって出力を埋めないため）。
constexpr std::size_t printed_faults = 8U;
constexpr std::size_t printed_bytes = 96U;

// 区切りの断片（Issue の 5 の族）。言語ごとに分けず、全言語に同じ断片の全部を通す（ほかの言語の
// 断片も、その言語にとっては想定外の入力として意味がある）。
constexpr auto fragments = std::to_array<std::string_view>({
    "\"",
    "'",
    "`",
    "\"\"\"",
    "'''",
    "/*",
    "*/",
    "//",
    "#",
    "<",
    ">",
    "$(",
    ")",
    "(",
    "<<EOF",
    "<<-EOF",
    "<<'EOF'",
    "EOF",
    "[[",
    "]]",
    "[=[",
    "]=]",
    "[==[",
    "]==]",
    "r#\"",
    "\"#",
    "r##\"",
    "\"##",
    "@\"",
    "$\"",
    "$@\"",
    "\"@",
    "@'",
    "'@",
    "<#",
    "#>",
    "#[",
    "#![",
    "]",
    "[",
    "{",
    "}",
    "${",
    "$",
    "\\",
    " ",
    "\t",
    "\r",
    std::string_view{"\0", 1U},
    "R\"x(",
    ")x\"",
    "R\"(",
    ")\"",
    "<!--",
    "-->",
    "<script>",
    "</script>",
    "<style>",
    "</style",
    "<![CDATA[",
    "]]>",
    "```",
    "~~~",
    "--",
    "--[[",
    "--[==[",
    "<?php",
    "<?=",
    "?>",
    "<<<EOT",
    "<<<'EOT'",
    "EOT",
    "$'",
    "((",
    "))",
    ";",
    "|",
    ": ",
    "- ",
    "0x",
    "1e",
    ".5",
    "b'",
    "'a'",
    "=",
    "/",
    "func ",
    "fn ",
    "function ",
    "RUN ",
    "set(",
    "x",
    "\xe3\x81\x82",
    "\xc3\xa9",
});

// fixture の行に似た見本（1 言語 10 行まで・LexerTests.cpp の case の行の形）。変異の元になる。
constexpr auto c_samples = std::to_array<std::string_view>({
    "int main(void) { return 0; } // done",
    "auto s = R\"xyz(raw text",
    "still raw )xyz\" + 1;",
    "/* open comment",
    "close */ x = 0x1F + 1.5e-3f;",
    "#include <vector>",
    "const char *c = \"a\\\"b\";",
    "std::string name = 'q';",
});
constexpr auto python_samples = std::to_array<std::string_view>({
    "def f(x): return x  # note",
    "s = \"\"\"open triple",
    "close\"\"\" + 'x'",
    "r'raw\\d' b\"bytes\" f\"{x}\"",
    "class A(B): pass",
    "x = 0x1f + 1_000 + 1e5j",
    "'''single",
});
constexpr auto json_samples = std::to_array<std::string_view>({
    "{\"key\": [1, -2.5e3, true, null]}",
    "  \"open string",
    "\"a\\u00e9\": false,",
    "]",
    "// comment",
    "{\"x\": {\"y\": \"z\"}}",
});
constexpr auto markdown_samples = std::to_array<std::string_view>({
    "# Heading",
    "```cpp",
    "int x = 0;",
    "```",
    "~~~~",
    "- item with `code` and **bold**",
    "> quote [link](http://x)",
    "    indented",
    "<div>html</div>",
});
constexpr auto javascript_samples = std::to_array<std::string_view>({
    "const s = `template ${x}",
    "still ${a + `inner`} end`;",
    "let r = /ab+c/g; let d = a / b;",
    "function f(a) { return a; }",
    "/* block",
    "*/ class C extends D {}",
    "const t: number = 1;",
    "<div className=\"x\">{y}</div>",
    "a </ b",
});
constexpr auto html_samples = std::to_array<std::string_view>({
    "<html lang=\"ja\"><!-- comment",
    "still -->",
    "<script>",
    "let x = '</div>';",
    "</script>",
    "<style>p { color: red; }</style>",
    "<![CDATA[ data",
    "]]>",
    "<?xml version=\"1.0\"?>",
});
constexpr auto css_samples = std::to_array<std::string_view>({
    "body { color: #fff; margin: 0 auto; }",
    "/* open",
    "*/ .a > .b::before { content: \"x\"; }",
    "@media (max-width: 600px) {",
    "  width: calc(100% - 2em);",
    "}",
    "a[href^='http'] { }",
});
constexpr auto yaml_samples = std::to_array<std::string_view>({
    "key: value # comment",
    "list:",
    "  - item",
    "block: |",
    "  text in block",
    "other: >-",
    "\"quoted\": 'single'",
    "anchor: &a *b",
});
constexpr auto toml_samples = std::to_array<std::string_view>({
    "[table]",
    "key = \"value\" # c",
    "s = \"\"\"multi",
    "line\"\"\"",
    "l = '''lit",
    "[[array]]",
    "d = 1979-05-27T07:32:00Z",
});
constexpr auto powershell_samples = std::to_array<std::string_view>({
    "function Get-Thing { param([int]$x) }",
    "<# block comment",
    "still inside #> $a = 1",
    "$s = @\"",
    "\"@ + 'x'",
    "$t = @'  ",
    "'@",
    "if ($a -eq 1) { Write-Host \"hi $(1 + 2)\" }",
});
constexpr auto shell_samples = std::to_array<std::string_view>({
    "f() {",
    "  local x=${1:-default}",
    "cat <<EOF",
    "body $x",
    "EOF",
    "out=$(ls -la | grep foo)",
    "echo 'open \\",
    "for i in 1 2 3; do echo $i; done",
    "(( a << 2 ))",
});
constexpr auto cmake_samples = std::to_array<std::string_view>({
    "cmake_minimum_required(VERSION 3.28)",
    "set(x \"a ${b} c\")",
    "#[[ block",
    "]] message(STATUS [=[ raw",
    "]=])",
    "if(ON AND NOT x-NOTFOUND)",
    "target_link_libraries(a PRIVATE $<TARGET_FILE:b>)",
});
constexpr auto gdscript_samples = std::to_array<std::string_view>({
    "func _ready() -> void:",
    "\tvar x: int = 0x1F",
    "\t$Path/To.show()",
    "@export var s = \"\"\"open",
    "close\"\"\"",
    "\"line one",
    "line two\"",
    "%Name & ^\"node\" &\"name\"",
});
constexpr auto dosini_samples = std::to_array<std::string_view>({
    "[section]",
    "; comment",
    "key = 10",
    "name = \"quoted\" value",
    "# hash",
    "x=1e5",
});
constexpr auto gdresource_samples = std::to_array<std::string_view>({
    "[gd_scene load_steps=3 format=3]",
    "[ext_resource type=\"Script\" path=\"res://a.gd\"]",
    "text = \"open",
    "close\"",
    "position = Vector2(1, -2)",
    "; comment",
    "visible = true",
});
constexpr auto cs_samples = std::to_array<std::string_view>({
    "public class A { void F() { } }",
    "var s = @\"verbatim",
    "still\"\"inside\";",
    "var r = \"\"\"",
    "raw\"\"\";",
    "var i = $\"{x} {\"y\"}\";",
    "#region Name",
    "/* block",
});
constexpr auto rust_samples = std::to_array<std::string_view>({
    "fn main() { println!(\"hi\"); }",
    "/* outer /* inner",
    "*/ still */",
    "let s = r##\"raw",
    "end\"##;",
    "let c = 'a'; let l: &'static str = \"x\";",
    "#[derive(Debug)]",
    "let b = b\"bytes\\\" open",
    "let n = 1_000u32 + 0b1010 + 1.5e-3f64;",
});
constexpr auto go_samples = std::to_array<std::string_view>({
    "func (r *T) Name() int { return 0 }",
    "s := `raw",
    "end`",
    "/* block",
    "*/ x := 'a' + 0x1p-2",
    "var m map[string]chan int",
    "\"open",
});
constexpr auto php_samples = std::to_array<std::string_view>({
    "<?php echo $x; ?>",
    "<p>Hello <?= $name ?></p>",
    "<?php $s = <<<EOT",
    "  body",
    "  EOT;",
    "/* start",
    "still */ echo 1;",
    "#[Attr(\"x\")] function f() {}",
    "$a->b::class; // c ?> after",
});
constexpr auto sql_samples = std::to_array<std::string_view>({
    "SELECT a, COUNT(*) FROM t WHERE b = 'x' -- c",
    "/* block",
    "*/ INSERT INTO \"t\" VALUES (1, NULL);",
    "'open",
    "close'",
    "CREATE TABLE `t` (id INT);",
});
constexpr auto lua_samples = std::to_array<std::string_view>({
    "local function f(a) return a end",
    "--[==[ long",
    "comment ]==]",
    "s = [[long",
    "string]]",
    "x = 0xA.8p-1 .. \"s\"",
    "function a.b:c() end",
});
constexpr auto dockerfile_samples = std::to_array<std::string_view>({
    "FROM ubuntu:24.04 AS build",
    "RUN apt-get update && \\",
    "    apt-get install -y x",
    "RUN <<EOF",
    "echo $HOME",
    "EOF",
    "# syntax=docker/dockerfile:1",
    "ENV A=\"b\" C=${D}",
});

// Language の値か（LanguageTable.cpp の is_language と同じ形）。値を足すとこの switch
// が落ち、足した後は 下の数え上げが 1 つ増えて last_language の static_assert が落ちる。
[[nodiscard]] consteval bool is_language(Language language) noexcept
{
    switch (language)
    {
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::c:
    case Language::cpp:
    case Language::python:
    case Language::json:
    case Language::javascript:
    case Language::typescript:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
    case Language::gdscript:
    case Language::dosini:
    case Language::gdresource:
    case Language::cs:
    case Language::rust:
    case Language::go:
    case Language::php:
    case Language::sql:
    case Language::lua:
    case Language::dockerfile:
        return true;
    }
    return false;
}

// 値は 0 から連続して並ぶので、最初に値でなくなる所までが値の数。
[[nodiscard]] consteval std::size_t language_count() noexcept
{
    std::size_t count = 0;
    while (is_language(static_cast<Language>(count)))
    {
        ++count;
    }
    return count;
}

// 言語のループの上限。Language の最後の値でなくなったらコンパイルが落ちる。
constexpr Language last_language = Language::dockerfile;
static_assert(std::to_underlying(last_language) + 1U == language_count(),
              "last_language is the last value of Language");

// 見本の行の供給元。新しい言語の値で switch が落ちる（CPP-002）。plain は見本を持たない。
[[nodiscard]] Lines samples_of(Language language)
{
    switch (language)
    {
    case Language::plain:
        return {};
    case Language::markdown:
        return markdown_samples;
    case Language::html:
    case Language::xml:
        return html_samples;
    case Language::c:
    case Language::cpp:
        return c_samples;
    case Language::python:
        return python_samples;
    case Language::json:
        return json_samples;
    case Language::javascript:
    case Language::typescript:
        return javascript_samples;
    case Language::yaml:
        return yaml_samples;
    case Language::toml:
        return toml_samples;
    case Language::css:
        return css_samples;
    case Language::powershell:
        return powershell_samples;
    case Language::shell:
        return shell_samples;
    case Language::cmake:
        return cmake_samples;
    case Language::gdscript:
        return gdscript_samples;
    case Language::dosini:
        return dosini_samples;
    case Language::gdresource:
        return gdresource_samples;
    case Language::cs:
        return cs_samples;
    case Language::rust:
        return rust_samples;
    case Language::go:
        return go_samples;
    case Language::php:
        return php_samples;
    case Language::sql:
        return sql_samples;
    case Language::lua:
        return lua_samples;
    case Language::dockerfile:
        return dockerfile_samples;
    }
    return {};
}

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

// 不正な UTF-8・NUL・CR・Tab を含むバイト。半分は打てる ASCII にして字句器の状態へ届かせる。
// LF は出さない: 字句器の行は改行を含まない（行の契約）。CR は LF 文書の literal CR
// として来るので残す。
[[nodiscard]] char random_byte(Random &random)
{
    constexpr std::string_view printable = " \t\r\"'`#/*\\<>$(){}[]@;:=-+.,0123456789abcxyzEOF";
    if (below(random, 2U) == 0U)
    {
        return printable[below(random, printable.size())];
    }
    // 0x00〜0xfe の 255 通りを引き、0x0a 以上を 1 つ上へずらして LF だけを飛ばす。
    const std::size_t byte = below(random, 255U);
    return static_cast<char>(byte < 0x0aU ? byte : byte + 1U);
}

[[nodiscard]] std::string random_line(Random &random, std::size_t length)
{
    std::string line;
    line.reserve(length);
    while (line.size() < length)
    {
        line.push_back(random_byte(random));
    }
    return line;
}

[[nodiscard]] std::string fragment_line(Random &random, std::size_t at_least)
{
    std::string line;
    while (line.size() < at_least)
    {
        line.append(fragments[below(random, fragments.size())]);
    }
    return line;
}

// 1 バイトの置換・削除・挿入・切り詰めのどれか 1 つ（剰余の 0〜3）。
[[nodiscard]] std::string mutated(Random &random, std::string_view sample)
{
    std::string line{sample};
    const std::size_t at = below(random, line.size() + 1U);
    const std::size_t kind = below(random, 4U);
    const bool inside = at < line.size();
    if (kind == 0U && inside)
    {
        line[at] = random_byte(random);
    }
    else if (kind == 1U && inside)
    {
        line.erase(at, 1U);
    }
    else if (kind == 2U)
    {
        line.insert(at, 1U, random_byte(random));
    }
    else if (kind == 3U)
    {
        line.resize(at);
    }
    return line;
}

void print_line(std::string_view line)
{
    std::fprintf(stderr, "  bytes(%zu):", line.size());
    for (std::size_t at = 0U; at < line.size() && at < printed_bytes; ++at)
    {
        std::fprintf(stderr, " %02x", static_cast<unsigned>(static_cast<unsigned char>(line[at])));
    }
    std::fprintf(stderr, "%s\n", line.size() > printed_bytes ? " ..." : "");
}

[[nodiscard]] bool state_closed(const LexState &state)
{
    return std::to_underlying(state.kind) <= std::to_underlying(LexKind::raw_string) &&
           state.delimiter_length <= core::raw_delimiter_limit;
}

// 区間は 1 桁以上。不正な UTF-8 の行（文書には来ない・TextBuffer が検証する）でも、字句器の桁は
// 不正なバイトを 1 つで 1 桁と数えるので 0 桁の区間は出ない（#878）。
[[nodiscard]] bool span_sound(const ColorSpan &span, std::size_t floor, std::size_t ceiling)
{
    return span.begin.value >= floor && span.begin.value < span.end.value &&
           span.end.value <= ceiling &&
           std::to_underlying(span.role) < std::to_underlying(SyntaxRole::warning);
}

// 不変量 1。桁は code point の序数で行頭が 1 なので、end は数 + 1 まで。
[[nodiscard]] bool spans_sound(const std::vector<ColorSpan> &spans, std::string_view line)
{
    const std::size_t ceiling =
        core::lenient_code_point_count(line, core::Offset{0U}, core::Offset{line.size()}) + 1U;
    std::size_t floor = 1U;
    for (const ColorSpan &span : spans)
    {
        if (!span_sound(span, floor, ceiling))
        {
            return false;
        }
        floor = span.end.value;
    }
    return true;
}

[[nodiscard]] const char *violation(Language language, const LexState &in, std::string_view line,
                                    const LexLine &lexed)
{
    if (!spans_sound(lexed.spans, line))
    {
        return "spans (invariant 1)";
    }
    if (core::lex_state(language, in, line) != lexed.out)
    {
        return "lex_state != lex_line.out (invariant 2)";
    }
    const LexLine again = core::lex_line(language, in, line);
    if (again.spans != lexed.spans || again.out != lexed.out)
    {
        return "not deterministic (invariant 3)";
    }
    if (!state_closed(lexed.out))
    {
        return "state out of the closed values (invariant 4)";
    }
    return nullptr;
}

// 1 行を検査して次の行へ渡す状態を返す。破れは faults に数え、最初の数件だけ書き出す。
[[nodiscard]] LexState checked_line(Language language, const LexState &in, std::string_view line,
                                    std::size_t &faults)
{
    const LexLine lexed = core::lex_line(language, in, line);
    const char *broken = violation(language, in, line, lexed);
    if (broken != nullptr)
    {
        if (faults < printed_faults)
        {
            std::fprintf(stderr, "lexer property: language %u, in kind %u delimiter %u: %s\n",
                         static_cast<unsigned>(std::to_underlying(language)),
                         static_cast<unsigned>(std::to_underlying(in.kind)),
                         static_cast<unsigned>(in.delimiter_length), broken);
            print_line(line);
        }
        ++faults;
    }
    return lexed.out;
}

// 字句器の桁の数え方（#878）。正しい UTF-8 では code_point_count と同じ。どの列にも入らない
// 継続バイト・先頭になれないバイトは 1 つで 1。区間を分けて数えた和は全体と同じ。
[[nodiscard]] std::size_t lenient(std::string_view text, std::size_t begin, std::size_t end)
{
    return core::lenient_code_point_count(text, core::Offset{begin}, core::Offset{end});
}

void verify_lenient_count()
{
    constexpr std::string_view valid = "a\xc3\xa9\xe3\x81\x82\xf0\x9f\x98\x80";
    constexpr std::string_view invalid = "\xa2\xc3\xa9\xa9\xc0\x80\xe3\x81";
    expect(lenient(valid, 0U, valid.size()) == core::code_point_count(valid) &&
               lenient(valid, 1U, 3U) == 1U && lenient(valid, 3U, 6U) == 1U,
           "lexer property: the lenient count equals the code point count on valid UTF-8");
    expect(lenient(invalid, 0U, invalid.size()) == 6U && lenient(invalid, 0U, 1U) == 1U &&
               lenient(invalid, 3U, 4U) == 1U && lenient(invalid, 5U, 6U) == 1U,
           "lexer property: every stray byte counts as one column");
    bool additive = true;
    for (std::size_t cut = 0U; cut <= invalid.size(); ++cut)
    {
        additive = additive && lenient(invalid, 0U, cut) + lenient(invalid, cut, invalid.size()) ==
                                   lenient(invalid, 0U, invalid.size());
    }
    expect(additive, "lexer property: the lenient count adds up across a cut");
}

// 50 倍の走りで最小化した入力（#868）。どれも不正な UTF-8 で、継続バイトだけの区間が 0 桁に
// なっていた（#878 の前）。不正なバイトも 1 桁と数え、区間が 1 桁以上で行の内に順に並ぶことを
// 決定的に残す。状態は字句器自身に作らせる（C++ の `/*`・JS と TS のテンプレートの `` ` ``）。
void verify_minimized_lines()
{
    std::size_t faults = 0U;
    const LexState cpp_block = core::lex_state(Language::cpp, LexState{}, "/*");
    const LexState template_open = core::lex_state(Language::javascript, LexState{}, "`");
    const LexState ts_template = core::lex_state(Language::typescript, LexState{}, "`");
    const LexState gd_string = core::lex_state(Language::gdscript, LexState{}, "\"");
    expect(cpp_block.kind == LexKind::block_comment && template_open.kind == LexKind::raw_string &&
               ts_template.kind == LexKind::raw_string && gd_string.kind == LexKind::raw_string,
           "lexer property: the minimized cases start inside a comment or a string");
    std::ignore = checked_line(Language::cpp, cpp_block, "\xa2", faults);
    std::ignore = checked_line(Language::javascript, template_open, "\x9a", faults);
    std::ignore = checked_line(Language::typescript, ts_template, "\x9a", faults);
    std::ignore = checked_line(Language::gdscript, gd_string, "\xbc\x8b", faults);
    std::ignore = checked_line(Language::html, LexState{}, "<b \xad", faults);
    std::ignore = checked_line(Language::xml, LexState{}, "<a\t\xb4", faults);
    std::ignore = checked_line(Language::css, LexState{}, "\x89:", faults);
    expect(faults == 0U, "lexer property: the minimized invalid UTF-8 lines stay inside the line");
}

// 文書のように続けて通す（不変量 4: in は字句器が前の行に出した値だけ）。
void run_document(Language language, std::span<const std::string> lines, bool from_first_line,
                  std::size_t &faults)
{
    LexState state = core::lex_start_state(language, from_first_line);
    for (const std::string &line : lines)
    {
        state = checked_line(language, state, line, faults);
    }
}

[[nodiscard]] std::vector<std::string> random_document(Random &random)
{
    std::vector<std::string> lines;
    for (std::size_t index = 0U; index < random_lines; ++index)
    {
        lines.push_back(random_line(random, below(random, 80U)));
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> fragment_document(Random &random)
{
    std::vector<std::string> lines;
    for (std::size_t index = 0U; index < fragment_lines; ++index)
    {
        lines.push_back(fragment_line(random, below(random, 48U)));
    }
    return lines;
}

// 見本の行を順に並べ、各行を 1 か所だけ変えた文書（変えない行も混ぜて状態を渡す）。
[[nodiscard]] std::vector<std::string> mutated_document(Random &random, Lines samples)
{
    std::vector<std::string> lines;
    for (std::size_t round = 0U; round < mutated_rounds; ++round)
    {
        for (const std::string_view sample : samples)
        {
            lines.push_back(below(random, 4U) == 0U ? std::string{sample}
                                                    : mutated(random, sample));
        }
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> long_document(Random &random, std::size_t length)
{
    std::vector<std::string> lines;
    for (std::size_t index = 0U; index < long_lines; ++index)
    {
        lines.push_back(fragment_line(random, length));
        lines.push_back(random_line(random, length));
    }
    return lines;
}

void verify_language(Language language, Random &random)
{
    std::size_t faults = 0U;
    run_document(language, random_document(random), true, faults);
    run_document(language, random_document(random), false, faults);
    run_document(language, fragment_document(random), true, faults);
    run_document(language, fragment_document(random), false, faults);
    run_document(language, mutated_document(random, samples_of(language)), true, faults);
    run_document(language, long_document(random, short_long_line), true, faults);
    expect(faults == 0U, "lexer property: every generated line keeps invariants 1-4");
}

// 字句器を持たない言語は区間を出さず状態を素通しする（ADR 0156 の決定 14）。1 本だけ見る。
void verify_language_without_lexer()
{
    constexpr std::string_view line{"/* \"x\" <<EOF \xff\0 [[", 18U};
    const LexState in = core::lex_start_state(Language::plain, true);
    const LexLine lexed = core::lex_line(Language::plain, in, line);
    expect(!core::has_lexer(Language::plain), "lexer property: plain has no lexer");
    expect(lexed.spans.empty() && lexed.out == in &&
               core::lex_state(Language::plain, in, line) == in,
           "lexer property: plain passes the state through without spans");
}

// 64 KiB の行は重いので言語ごとに 1 本（巨大な行の 16,384 code point を超える所は application
// が区間を付けないが、字句器は落ちず区間が行の内にあること）。
void verify_long_line(Language language, Random &random)
{
    std::size_t faults = 0U;
    const std::array<std::string, 1> lines{fragment_line(random, long_long_line)};
    run_document(language, lines, true, faults);
    expect(faults == 0U, "lexer property: a 64 KiB line keeps invariants 1-4");
}
} // namespace

void verify_lexer_property_scope()
{
    verify_lenient_count();
    verify_minimized_lines();
    verify_language_without_lexer();
    Random random{property_seed};
    // 上限の last_language は static_assert が Language の最後の値に縛る。
    for (std::underlying_type_t<Language> value = 0U; value <= std::to_underlying(last_language);
         ++value)
    {
        const auto language = static_cast<Language>(value);
        if (core::has_lexer(language))
        {
            verify_language(language, random);
            verify_long_line(language, random);
        }
    }
}
} // namespace nenenib::tests
