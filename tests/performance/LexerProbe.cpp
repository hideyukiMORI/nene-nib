#include "LexerProbe.hpp"

#include "Language.hpp"
#include "LexKind.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"
#include "Lexer.hpp"
#include "Milestone.hpp"
#include "ProbeWorkload.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t line_bytes = 100U;
constexpr std::size_t template_count = 6U;
// 最悪形の 1 行の長さ（#836 の R1・eng/compare-probes.py の LEXER_WORST_LINE_BYTES と同じ）。
constexpr std::size_t worst_line_bytes = 16'384U;
// 入力は eng/compare-probes.py の lexer_input と同じ生成の形。C++ と Python は 2 行目が行を
// またぐ字句（ブロックコメント・3 連引用符の文字列）を開き 3 行目が閉じる。JSON は行をまたがない。
// C++: raw string・文字列・数字・演算子・前処理を含む。
constexpr std::array<std::string_view, template_count> cpp_templates{
    "#include <vector> // container",
    "static constexpr std::uint32_t mask = 0xFF'FFu; /* block comment begins",
    "   continues the block comment */ int value = compute(12.5e-3f, 'x');",
    "auto text = R\"raw(raw string with ) inside)raw\"; if (text.size() >= 10) { return; }",
    "for (int i = 0; i < count; ++i) { total += values[i] * 2; } // loop",
    "const char *name = \"string with \\\" escape\"; bool ok = name != nullptr && true;"};
// JSON（#830）: 入れ子・エスケープ・数字の形・定数・裸のキー。
constexpr std::array<std::string_view, template_count> json_templates{
    "{\"name\": \"nene-nib\", \"version\": \"0.1.0\", \"private\": true, \"count\": 12,",
    "  \"values\": [1, -2.5, 3e10, 4.0E-3, null, false], \"nested\": {\"a\": {\"b\": [ ]}},",
    "  \"escaped\": \"quote \\\" and backslash \\\\ and \\u00e9\", \"unicode\": "
    "\"\\u65e5\\u672c\",",
    "  bareKey: \"not strict json\", \"list\": [{\"x\": 1}, {\"y\": 2}, {\"z\": 3}],",
    "  \"path\": \"C:\\\\work\\\\nib\", \"empty\": \"\", \"zero\": 0, \"neg\": -0.125,",
    "  \"last\": [true, false, null], \"end\": {} },"};
// Python（#830）: コメント・接頭辞つき文字列・3
// 連引用符・数字・予約語・定数・型・関数・デコレータ。
constexpr std::array<std::string_view, template_count> python_templates{
    "def compute(value: int, *, scale: float = 1.5e3) -> list[int]:  # compute",
    "    doc = r\"\"\"raw triple string opens here with 'quotes' and \\d escapes",
    "    still inside\"\"\" + f\"{value!r}\" + b'bytes' + 'it\\'s'; n = 0x_FF_FF + 1_000j",
    "@functools.lru_cache(maxsize=None)",
    "    if value is not None and isinstance(value, dict): return [len(v) for v in value]",
    "class Widget(object): pass  # trailing comment with \"quotes\" and 'apostrophes'"};
// Markdown（#833）: 見出し・強調・行内コード・リンクと画像・タグ・引用・箇条書き。
// 2 行目がフェンスを開き 3 行目が閉じる（閉じの行は後ろが空白だけなので、詰め物は空白）。
constexpr std::array<std::string_view, template_count> markdown_templates{
    "# Heading with **bold** and `code` and [link](https://example.com/path)",
    "```cpp fenced code block opens here with an info string",
    "```",
    "- list item with *emphasis* and __strong__ and ![image](img.png) and <span>tag</span>",
    "> quoted text with `inline code` and a [reference link](#anchor) and more words",
    "1. ordered item with _italic_ text, an escaped \\* star, and plain prose to the end"};
// JavaScript（#833）: 正規表現と除算・ブロックコメント・テンプレート・数字の形・予約語・
// 定数・演算子。
constexpr std::array<std::string_view, template_count> javascript_templates{
    "import { readFile } from 'node:fs'; const pattern = /ab+c\\/[a-z/]+/gi; // regex",
    "export async function compute(value, scale = 1.5e3) { /* block comment opens",
    "   closes here */ let total = (value / 2) ** 3 + 0xFF + 1_000n; return total;",
    "const text = `template ${value} literal` + \"double \\\" quote\" + 'single';",
    "if (count >= 10 && ready !== null) { items?.push(...rest); } else { x ?\?= undefined; }",
    "return typeof value === 'string' ? value.split(/\\s+/) : [true, false, NaN];"};

// HTML（#838）: 宣言・タグ・属性（引用符の中の `>`・引用符の無い値）・実体参照。2 行目がコメント
// を開き 3 行目が閉じる。
constexpr std::array<std::string_view, template_count> html_templates{
    "<!DOCTYPE html> <html lang=\"ja\"><head><meta charset=\"utf-8\"><title>Nib &amp; "
    "co</title>",
    "<body class=\"page\" data-id='42'> <!-- comment block opens here and continues",
    "   still in the comment --> <div id=main><p>text &lt; more &#169;</p></div>",
    "<a href=\"https://example.com/a>b\" title='link'>anchor</a> <img src=x.png alt=\"pic\" />",
    "<ul><li class=\"item\">one</li><li class=\"item\">two</li><li>three &nbsp;</li></ul>",
    "<input type=\"checkbox\" checked disabled> <br/> <span>plain text body</span>"};
// XML（#838）: `<?xml ?>`・名前空間・CDATA（2 行目が開き 3 行目が閉じる）・宣言・実体参照。
constexpr std::array<std::string_view, template_count> xml_templates{
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?> <root xmlns:x=\"urn:example:ns\">",
    "<x:item id=\"1\" kind='a'><![CDATA[ raw <data> & text opens here and continues",
    "   more raw text ]]></x:item> <!-- note --> <x:empty attr=\"v\"/>",
    "<entry key=\"name\" value=\"nene-nib\" enabled=\"true\">text &amp; &#x41;</entry>",
    "<!DOCTYPE note SYSTEM \"note.dtd\"> <list><e>1</e><e>2</e><e>3</e></list>",
    "<?pi target?> <a:b.c-d e=\"f\" g='h'>mixed content here</a:b.c-d> </root>"};
// CSS（#838）: at-rule・セレクタ・プロパティと値・単位・色・`!important`・文字列。2 行目がコメント
// を開き 3 行目が閉じる。
constexpr std::array<std::string_view, template_count> css_templates{
    "@import url(\"theme.css\"); @media screen and (max-width: 600px) { body { margin: 0 } }",
    ".item > a:hover, #main-nav { color: #a0b0c0; /* block comment opens here and continues",
    "   still comment */ padding: 12px 1.5em 50% .5rem; border: 1px solid #fff !important; }",
    "h1 { font-family: \"Noto Sans JP\", sans-serif; font-size: 2em; line-height: 1.5; }",
    "div::before { content: \"x\"; --main-color: #333; -webkit-transition: all 0.3s; }",
    "p{color:red;width:calc(100% - 2px);transform:rotate(45deg);z-index:10}"};
// YAML（#838）: キー・文字列・flow・定数・数・アンカー。2 行目がブロックスカラーを開き、3 行目は
// その中（深い字下げ）、4 行目が字下げで戻る。
constexpr std::array<std::string_view, template_count> yaml_templates{
    "name: \"nene-nib\" # the project name with a quoted string value and comment",
    "description: >- # a folded block scalar opens here",
    "    folded text continues with deeper indentation: not a key, 42, true",
    "list: [1, two, \"3\", true, null, ~, 0xFF, .inf, -1.5e+3] # flow sequence",
    "- key: &anchor {a: 1, b: x, c: 'it''s'} # list item with anchor and flow",
    "  ref: *anchor # alias reference and a trailing comment for good measure"};
// TOML（#838）: 見出し・キー・3 連引用符（2 行目が開き 3 行目が閉じる）・インラインテーブル・数と
// 日時。
constexpr std::array<std::string_view, template_count> toml_templates{
    "[package] # table header with a trailing comment",
    "description = \"\"\"multi-line basic string opens here with \"quotes\" and \\t escapes",
    "   still inside the string\"\"\" # closes the multi-line string",
    "inline = { x = 1, y.z = \"w\", flag = true } # inline table with dotted key",
    "numbers = [ 1_000, 0xDEAD_BEEF, 6.626e-34, -inf, nan, +99 ] # array of numbers",
    "when = 1979-05-27T07:32:00-08:00 # offset date-time and 'literal' text"};
// GDScript（#855）: 注釈・型の注釈・3 連引用符（2 行目が開き 3 行目が閉じる）・StringName と
// NodePath・ノードのパス・`%`・数の形。
constexpr std::array<std::string_view, template_count> gdscript_templates{
    "@export var speed: float = 120.5  # exported with a type annotation and a default",
    "    var doc := \"\"\"multi-line string opens here with 'quotes' and \\t escapes",
    "    still inside\"\"\" + &\"name\" + ^\"Path/To:prop\"; var n = $Path/To/Node",
    "func move(delta: float, dir: Vector2) -> void: position += dir * speed * delta",
    "    if hp % 2 == 0 and not self.dead: await get_tree().create_timer(1.5).timeout",
    "    match state: State.IDLE when %Label.visible: return 0xFF_FF + 1_000 + .5"};
// Ini（#855・dosini）: 見出し・空白の入ったキー・数の値・コメント・文字列。行をまたがない。
constexpr std::array<std::string_view, template_count> dosini_templates{
    "[Section Name] ; a section header followed by trailing text",
    "key with spaces = some plain value text that is not a number at all",
    "count = 42",
    "; comment line with = equals and \"quotes\" inside it",
    "name = \"quoted value\" and \"another one\" in the same line",
    "path=C:\\Windows\\System32\\drivers\\etc ; not a comment in dosini"};
// Godot の設定（#855・gdresource）: 見出しと属性・行をまたぐ `"…"`（2 行目が開き 3 行目が閉じる）・
// 呼び出しの名前・数・StringName と NodePath・定数・`;` のコメント。
constexpr std::array<std::string_view, template_count> gdresource_templates{
    "[gd_scene load_steps=3 format=3 uid=\"uid://cvx1a2b3c4d5\"]",
    "script/source = \"extends Node2D multi-line string opens here with \\\"escaped\\\" quotes",
    "func _ready(): pass\" ; closes the embedded script string",
    "[node name=\"Player\" type=\"CharacterBody2D\" parent=\".\" groups=[\"solid\", \"wall\"]]",
    "points = PackedVector2Array(-8, -8, 8, -8, 1e+06, .25) ; polygon points",
    "config/features=PackedStringArray(\"4.3\", \"GL Compatibility\"), &\"jump\", ^\"../Player\", "
    "true"};
// C#（#857）: 宣言・逐語的文字列（2 行目が開き 3 行目が閉じる）・生の文字列・数・補間・文字・
// `#region` の行。
constexpr std::array<std::string_view, template_count> cs_templates{
    "using System.Text; namespace Demo.App { public sealed class Player : IDisposable {",
    "    var doc = @\"verbatim string opens here with \"\"doubled\"\" quotes and C:\\path",
    "    continues the verbatim\"; var raw = \"\"\"raw \"quoted\" text\"\"\"; int n = 0x_FF;",
    "    public async Task<int> RunAsync(string name) { await Task.Delay(1500); return 1; }",
    "    var s = $\"{name} has {count:N2} items\"; char c = '\\n'; decimal m = 1.5m;",
    "#region Helpers for the player class"};
// Rust（#857）: 属性・ライフタイム・入れ子のコメント（2 行目が開き 3 行目が閉じる）・生の文字列・
// 数・マクロ・文字とバイト文字・`match`。
constexpr std::array<std::string_view, template_count> rust_templates{
    "use std::collections::HashMap; #[derive(Debug, Clone)] pub struct Point { x: f64 }",
    "fn area<'a>(s: &'a str) -> usize { /* outer /* nested */ still inside the outer",
    "   ends here */ let r = r#\"raw \"quoted\" text\"#; let n = 1_000u32 + 0b1010; 0 }",
    "impl Shape for Point { fn name(&self) -> String { println!(\"{}\", self.x); todo!() } }",
    "let c = 'a'; let b = b'z'; let lt: &'static str = \"text\"; let o = Some(Self::new());",
    "match value { None => 0, Some(v) if v != 0 => v * 2, _ => -1 }"};
// Go（#857）: `package` `import`・メソッドの受け手・生の文字列（2 行目が開き 3 行目が閉じる）・
// 数の形・組み込みの関数・rune・`iota` `nil`。
constexpr std::array<std::string_view, template_count> go_templates{
    "package main; import (\"fmt\"; str \"strings\"); type Point struct { X, Y int }",
    "func (p *Point) Move(dx int) error { raw := `raw string opens with \"quotes\" and \\n",
    "  still raw` + \"done\"; x := 0x1p-2 + 1e3 + .5; return nil",
    "func main() { xs := make([]int, 0, 10); xs = append(xs, len(xs), cap(xs)); fmt.Println(xs) }",
    "    var r rune = 'a'; const ( A = iota; B ); if err != nil { panic(err) }",
    "    for i := range 10 { go func() { ch <- true }(); defer close(ch) }"};

[[nodiscard]] constexpr core::Language language_of(LexerWorkload workload)
{
    switch (workload)
    {
    case LexerWorkload::cpp_visible_60:
    case LexerWorkload::cpp_backfill_200:
        return core::Language::cpp;
    case LexerWorkload::json_visible_60:
    case LexerWorkload::json_backfill_200:
        return core::Language::json;
    case LexerWorkload::python_visible_60:
    case LexerWorkload::python_backfill_200:
        return core::Language::python;
    case LexerWorkload::markdown_visible_60:
    case LexerWorkload::markdown_backfill_200:
    case LexerWorkload::markdown_worst_line:
        return core::Language::markdown;
    case LexerWorkload::javascript_visible_60:
    case LexerWorkload::javascript_backfill_200:
        return core::Language::javascript;
    case LexerWorkload::html_visible_60:
    case LexerWorkload::html_backfill_200:
        return core::Language::html;
    case LexerWorkload::xml_visible_60:
        return core::Language::xml;
    case LexerWorkload::css_visible_60:
        return core::Language::css;
    case LexerWorkload::yaml_visible_60:
    case LexerWorkload::yaml_backfill_200:
        return core::Language::yaml;
    case LexerWorkload::toml_visible_60:
    case LexerWorkload::toml_backfill_200:
        return core::Language::toml;
    case LexerWorkload::gdscript_visible_60:
    case LexerWorkload::gdscript_backfill_200:
        return core::Language::gdscript;
    case LexerWorkload::dosini_visible_60:
        return core::Language::dosini;
    case LexerWorkload::gdresource_visible_60:
    case LexerWorkload::gdresource_backfill_200:
        return core::Language::gdresource;
    case LexerWorkload::cs_visible_60:
    case LexerWorkload::cs_backfill_200:
        return core::Language::cs;
    case LexerWorkload::rust_visible_60:
    case LexerWorkload::rust_backfill_200:
        return core::Language::rust;
    case LexerWorkload::go_visible_60:
    case LexerWorkload::go_backfill_200:
        return core::Language::go;
    }
    std::unreachable();
}

[[nodiscard]] constexpr bool is_visible(LexerWorkload workload)
{
    constexpr std::array<LexerWorkload, 17> visible{
        LexerWorkload::cpp_visible_60,      LexerWorkload::json_visible_60,
        LexerWorkload::python_visible_60,   LexerWorkload::markdown_visible_60,
        LexerWorkload::markdown_worst_line, LexerWorkload::javascript_visible_60,
        LexerWorkload::html_visible_60,     LexerWorkload::xml_visible_60,
        LexerWorkload::css_visible_60,      LexerWorkload::yaml_visible_60,
        LexerWorkload::toml_visible_60,     LexerWorkload::gdscript_visible_60,
        LexerWorkload::dosini_visible_60,   LexerWorkload::gdresource_visible_60,
        LexerWorkload::cs_visible_60,       LexerWorkload::rust_visible_60,
        LexerWorkload::go_visible_60};
    return std::ranges::find(visible, workload) != visible.end();
}

[[nodiscard]] constexpr std::size_t line_count(LexerWorkload workload)
{
    return is_visible(workload) ? 60U : 200U;
}

[[nodiscard]] constexpr std::span<const std::string_view> templates_of(LexerWorkload workload)
{
    constexpr std::array<std::pair<core::Language, std::span<const std::string_view>>, 15> tables{
        {{core::Language::json, json_templates},
         {core::Language::markdown, markdown_templates},
         {core::Language::javascript, javascript_templates},
         {core::Language::python, python_templates},
         {core::Language::html, html_templates},
         {core::Language::xml, xml_templates},
         {core::Language::css, css_templates},
         {core::Language::yaml, yaml_templates},
         {core::Language::toml, toml_templates},
         {core::Language::gdscript, gdscript_templates},
         {core::Language::dosini, dosini_templates},
         {core::Language::gdresource, gdresource_templates},
         {core::Language::cs, cs_templates},
         {core::Language::rust, rust_templates},
         {core::Language::go, go_templates}}};
    const core::Language language = language_of(workload);
    for (const auto &[table_language, templates] : tables)
    {
        if (table_language == language)
        {
            return templates;
        }
    }
    return cpp_templates;
}

// 各行は「型の行 + 言語の行末コメントの印（JSON・Markdown・HTML・XML・CSS・Ini は空白・
// gdresource は ` ;`・C++・JavaScript・C#・Rust・Go は ` //`）+ 詰め物」で 100
// バイトにそろえる。詰め物は `-` で、Markdown だけ空白 （フェンスの閉じの行を閉じのまま保つ）。
[[nodiscard]] constexpr std::string_view suffix_of(LexerWorkload workload)
{
    const core::Language language = language_of(workload);
    const bool hashed = language == core::Language::python || language == core::Language::yaml ||
                        language == core::Language::toml || language == core::Language::gdscript;
    const bool slashed = language == core::Language::cpp ||
                         language == core::Language::javascript || language == core::Language::cs ||
                         language == core::Language::rust || language == core::Language::go;
    if (language == core::Language::gdresource)
    {
        return " ;";
    }
    if (hashed)
    {
        return " #";
    }
    return slashed ? " //" : " ";
}

[[nodiscard]] constexpr char padding_of(LexerWorkload workload)
{
    return language_of(workload) == core::Language::markdown ? ' ' : '-';
}

// 2 行目の型の後ろで行をまたぐ状態に入るか（C++ と JavaScript と CSS のブロックコメント・Python と
// TOML の 3 連引用符・Markdown のフェンス・HTML のコメント・XML の CDATA・YAML
// のブロックスカラー）。 YAML だけは 3 行目（深い字下げの中身）でもまだ中にいて、4 行目で戻る。
[[nodiscard]] constexpr bool opened_after(LexerWorkload workload, std::size_t index)
{
    const core::Language language = language_of(workload);
    const std::size_t row = index % template_count;
    const bool single_line = language == core::Language::json || language == core::Language::dosini;
    return !single_line && (row == 1U || (language == core::Language::yaml && row == 2U));
}

[[nodiscard]] std::string padded(std::string_view line, std::string_view suffix, char padding)
{
    std::string text{line};
    text += suffix;
    text.append(line_bytes - text.size(), padding);
    return text;
}

[[nodiscard]] std::vector<std::string_view> lines_of(const std::string &input)
{
    std::vector<std::string_view> lines;
    const std::string_view text{input};
    for (std::size_t at = 0; at < text.size();)
    {
        const std::size_t end = text.find('\n', at);
        lines.push_back(text.substr(at, end - at));
        at = end + 1U;
    }
    return lines;
}

// 行ごとの区間の数と終わりの桁、出る状態の種類を足す。
[[nodiscard]] std::uint64_t lexed_sum(const core::LexLine &lexed, bool opens, bool &valid)
{
    valid = valid && (lexed.out.kind != core::LexKind::plain) == opens;
    std::uint64_t sum = lexed.spans.size() + static_cast<std::uint64_t>(lexed.out.kind);
    for (const auto &span : lexed.spans)
    {
        sum += span.end.value;
    }
    return sum;
}
} // namespace

std::string lexer_input(LexerWorkload workload)
{
    if (workload == LexerWorkload::markdown_worst_line)
    {
        return std::string(worst_line_bytes, '[') + '\n';
    }
    const std::span<const std::string_view> templates = templates_of(workload);
    std::string text;
    text.reserve(line_count(workload) * (line_bytes + 1U));
    for (std::size_t index = 0; index < line_count(workload); ++index)
    {
        text +=
            padded(templates[index % templates.size()], suffix_of(workload), padding_of(workload));
        text += '\n';
    }
    return text;
}

// visible は lex_line で区間ごと足し、backfill は lex_state で状態だけを次の行へ渡す（段 2
// の遡り）。
std::expected<std::uint64_t, ProbeFailure>
lexed_lines(LexerWorkload workload, const std::string &input, application::TimingPort &timing)
{
    if (input != lexer_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::vector<std::string_view> lines = lines_of(input);
    const bool visible = is_visible(workload);
    const core::Language language = language_of(workload);
    std::cerr << "preparedBytes=" << input.size() << " lines=" << lines.size() << '\n';
    core::LexState state{};
    std::uint64_t checksum = checksum_of(input);
    bool valid = true;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; visible && index < lines.size(); ++index)
    {
        const core::LexLine lexed = core::lex_line(language, state, lines[index]);
        checksum += lexed_sum(lexed, opened_after(workload, index), valid);
        state = lexed.out;
    }
    for (std::size_t index = 0; !visible && index < lines.size(); ++index)
    {
        state = core::lex_state(language, state, lines[index]);
        checksum += static_cast<std::uint64_t>(state.kind);
    }
    timing.mark(core::Milestone::probe_finished);
    // 最後の行が 2 行目の型なら行をまたぐ状態の中で終わる（200 行は 199 % 6 == 1）。
    const bool open_at_end = opened_after(workload, lines.size() - 1U);
    if (!valid || (state.kind != core::LexKind::plain) != open_at_end)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
