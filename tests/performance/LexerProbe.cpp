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
// PHP（#860）: `<?php` で入る宣言・属性とブロックコメント（2 行目が開き 3 行目が閉じる）・
// `$this->x`・3 種の文字列・数の形・`?>` の後の HTML と `<?=` と `<?php` で戻る行。
constexpr std::array<std::string_view, template_count> php_templates{
    "<?php namespace App\\Models; use App\\Contracts\\Shape; final class Circle extends Base {",
    "    #[Pure] public function area(float $r = 1.5): float { /* block comment begins",
    "   ends here */ return 3.14 * $this->radius ** 2; } private ?string $name = null;",
    "    $s = 'single' . \"double {$x}\" . `cmd`; $a = TRUE || Null ?? false; echo __LINE__;",
    "    $f = fn($y) => $y + 0x1F + 1_000 + .5; if (isset($list[0])) { return new static(); }",
    "    echo 1; ?> <b>html</b> <?= $name ?> <?php $v = match($x) { 1 => 'a', default => 'b' };"};
// SQL（#860）: 結合と集約・ブロックコメント（2 行目が開き 3 行目が閉じる）・`CREATE TABLE`
// と型・`'it''s'` と `"quoted"`・`CASE` と関数。
constexpr std::array<std::string_view, template_count> sql_templates{
    "SELECT u.id, u.name, COUNT(*) AS n FROM users u LEFT JOIN orders o ON o.user_id = u.id",
    "WHERE u.active = TRUE AND o.total > 10.5 /* block comment begins here and",
    "   continues */ GROUP BY u.id, u.name HAVING SUM(o.total) >= 100 ORDER BY n DESC;",
    "CREATE TABLE items (id INTEGER PRIMARY KEY, name VARCHAR(255) NOT NULL DEFAULT '');",
    "INSERT INTO items VALUES (1, 'it''s', 3.5e-2), (2, \"quoted\", NULL); UPDATE t SET x = 1;",
    "SELECT CASE WHEN x <> 0 THEN 'y' ELSE 'n' END, COALESCE(a, 0) FROM t WHERE b BETWEEN 1 AND "
    "2;"};
// Lua（#860）: `local function`・長いコメント `--[==[`（2 行目が開き 3 行目が閉じる）・
// 長い文字列・`..`・数の形・`goto` とラベル・`function obj:method`。
constexpr std::array<std::string_view, template_count> lua_templates{
    "local function greet(name) return \"Hello, \" .. name .. '!' end local t = { 1, 2.5, 0x1p4 }",
    "function M.helper(x, y) --[==[ long comment begins here and",
    "   continues ]==] if x == nil then return false elseif x ~= y then return true end end",
    "local s = [[long string]] .. \"esc \\\" quote\" for i = 1, 10 do t[i] = #t // 2 % 3 ^ 2 end",
    "while not done and ok or fail do repeat n = n - 1 until n <= 0 goto skip end ::skip::",
    "function obj:method(...) local a <const> = 5 return setmetatable({}, self), nil, 3e10 end"};
// Dockerfile（#860）: パーサ指示・`FROM … AS`・行末の `\` の続き（3 行目が続き 4 行目で
// 終わる・plain の種類のまま）・`$VAR` `${VAR}`・JSON の形の `CMD`。行をまたぐ種類に入る
// 行が無いので、詰め物は空白（続きの `\` を行末に保つ）。
constexpr std::array<std::string_view, template_count> dockerfile_templates{
    "# syntax=docker/dockerfile:1",
    "FROM golang:1.22 AS build",
    "RUN apk add --no-cache git curl ca-certificates && \\",
    "    go build -o /out/app ./cmd/app && echo \"built $VERSION\"",
    "ENV PATH=$PATH:/opt/bin HOME=${HOME:-/root} LABEL=\"nene \\\"nib\\\"\"",
    "CMD [\"./app\", \"--port\", \"8080\"]"};
// PowerShell（#862）: `function Get-Thing` と `param` の型・`<# #>`（2 行目が開き 3
// 行目が閉じる）・
// `-eq` `-NotLike`・`1kb` `0x1F`・`` `" `` の続く文字列・`@{}` と `[System.IO.File]::`・`try`。
constexpr std::array<std::string_view, template_count> powershell_templates{
    "function Get-Thing { param([string]$Path = $env:TEMP, [int]$Count = 0x1F) Get-ChildItem $Path",
    "    if ($Count -eq 1kb -and $Path -NotLike 'a*') { return $true } <# block comment begins",
    "   ends here #> foreach ($i in 1..10) { $s = \"item `\"$i`\"\"; Write-Output $s }",
    "    $h = @{ Name = 'it''s'; Size = 1.5e3 }; [System.IO.File]::ReadAllText($h.Name) | Out-Null",
    "    try { throw 'x' } catch [Exception] { $null = $_ } finally { $x = -not $false -or $?",
    "} class Point { [int]$X } enum Color { Red }; Switch -Regex ($s) { default { break } }"};
// shell（#862）: `f() {`・`${1:-default}`・`a#b`・`"…"` の行またぎ（2 行目が開き 3 行目が
// 閉じる）・`$(…)` と `` `…` ``・`case … esac`・算術の `<<`・`<<<`・`$'…'`。
constexpr std::array<std::string_view, template_count> shell_templates{
    "set -euo pipefail; f() { local x=${1:-default}; echo \"$x\" | grep -v a#b; return 0; }",
    "if [[ -f \"$file\" ]]; then echo 'single' $(ls -la | wc -l) `date` \"multi line begins",
    "   continues here\"; elif [ -d x ]; then exit 1; else printf '%s\\n' \"$@\"; fi",
    "case \"$1\" in start|stop) run \"$1\" ;; *) exit 2 ;; esac; for i in 1 2 3; do :; done",
    "while read -r line; do echo $((i<<2)) $? $$ $!; done < file; grep x <<< \"$v\"",
    "export PATH=\"$PATH:/bin\"; time sleep 1; echo $'a\\'b' ${HOME}/x; function greet { true; }"};
// CMake（#862）: コマンドと制御のコマンド・`#[==[ ]==]`（2 行目が開き 3 行目が閉じる）・
// `${…}` `$ENV{…}` `$CACHE{…}` `$<…>`・`ON` `off` `*-NOTFOUND`・括弧の引数・数。
constexpr std::array<std::string_view, template_count> cmake_templates{
    "cmake_minimum_required(VERSION 3.28) # project(nenenib LANGUAGES CXX) is set below",
    "add_executable(nib src/main.cpp ${SOURCES} $<TARGET_FILE:x>) #[==[ bracket comment begins",
    "  continues here ]==] set(FLAG ON) option(USE_X \"Use x ${Y}\" off) set(Q [[raw]])",
    "if(WIN32 AND NOT X-NOTFOUND) target_compile_options(nib PRIVATE /W4 $ENV{CFLAGS} 1.2.3)",
    "foreach(item IN LISTS items) message(STATUS \"item: ${item} \\\"q\\\"\") endforeach()",
    "target_link_libraries(nib PRIVATE $<$<CONFIG:Debug>:-g> $CACHE{LIB}) set(Y yes TRUE)"};

// LexerWorkload の値か。値を足すとこの switch がコンパイルで落ち（CPP-002）、足した後は下の
// 数え上げが 1 つ増えて、言語の表に行が無ければ static_assert が落ちる（LanguageTable.cpp と同じ
// 形・#860 で分岐の関数が 60 行を越えたので表にした）。
[[nodiscard]] consteval bool is_workload(LexerWorkload workload) noexcept
{
    switch (workload)
    {
    case LexerWorkload::cpp_visible_60:
    case LexerWorkload::cpp_backfill_200:
    case LexerWorkload::json_visible_60:
    case LexerWorkload::json_backfill_200:
    case LexerWorkload::python_visible_60:
    case LexerWorkload::python_backfill_200:
    case LexerWorkload::markdown_visible_60:
    case LexerWorkload::markdown_backfill_200:
    case LexerWorkload::javascript_visible_60:
    case LexerWorkload::javascript_backfill_200:
    case LexerWorkload::markdown_worst_line:
    case LexerWorkload::html_visible_60:
    case LexerWorkload::html_backfill_200:
    case LexerWorkload::xml_visible_60:
    case LexerWorkload::css_visible_60:
    case LexerWorkload::yaml_visible_60:
    case LexerWorkload::yaml_backfill_200:
    case LexerWorkload::toml_visible_60:
    case LexerWorkload::toml_backfill_200:
    case LexerWorkload::gdscript_visible_60:
    case LexerWorkload::gdscript_backfill_200:
    case LexerWorkload::dosini_visible_60:
    case LexerWorkload::gdresource_visible_60:
    case LexerWorkload::gdresource_backfill_200:
    case LexerWorkload::cs_visible_60:
    case LexerWorkload::cs_backfill_200:
    case LexerWorkload::rust_visible_60:
    case LexerWorkload::rust_backfill_200:
    case LexerWorkload::go_visible_60:
    case LexerWorkload::go_backfill_200:
    case LexerWorkload::php_visible_60:
    case LexerWorkload::php_backfill_200:
    case LexerWorkload::sql_visible_60:
    case LexerWorkload::lua_visible_60:
    case LexerWorkload::lua_backfill_200:
    case LexerWorkload::dockerfile_visible_60:
    case LexerWorkload::powershell_visible_60:
    case LexerWorkload::powershell_backfill_200:
    case LexerWorkload::shell_visible_60:
    case LexerWorkload::shell_backfill_200:
    case LexerWorkload::cmake_visible_60:
    case LexerWorkload::cmake_backfill_200:
        return true;
    }
    return false;
}

// 値は 0 から連続して並ぶので、最初に値でなくなる所までが値の数。
[[nodiscard]] consteval std::size_t workload_count() noexcept
{
    std::size_t count = 0;
    while (is_workload(static_cast<LexerWorkload>(count)))
    {
        ++count;
    }
    return count;
}

// i 行目が i 番目の LexerWorkload の言語。
constexpr std::array<core::Language, 42> workload_languages{
    core::Language::cpp,        core::Language::cpp,        core::Language::json,
    core::Language::json,       core::Language::python,     core::Language::python,
    core::Language::markdown,   core::Language::markdown,   core::Language::javascript,
    core::Language::javascript, core::Language::markdown,   core::Language::html,
    core::Language::html,       core::Language::xml,        core::Language::css,
    core::Language::yaml,       core::Language::yaml,       core::Language::toml,
    core::Language::toml,       core::Language::gdscript,   core::Language::gdscript,
    core::Language::dosini,     core::Language::gdresource, core::Language::gdresource,
    core::Language::cs,         core::Language::cs,         core::Language::rust,
    core::Language::rust,       core::Language::go,         core::Language::go,
    core::Language::php,        core::Language::php,        core::Language::sql,
    core::Language::lua,        core::Language::lua,        core::Language::dockerfile,
    core::Language::powershell, core::Language::powershell, core::Language::shell,
    core::Language::shell,      core::Language::cmake,      core::Language::cmake};
static_assert(workload_languages.size() == workload_count(),
              "workload_languages has one row for every LexerWorkload");

[[nodiscard]] constexpr core::Language language_of(LexerWorkload workload)
{
    return workload_languages.at(std::to_underlying(workload));
}

[[nodiscard]] constexpr bool is_visible(LexerWorkload workload)
{
    constexpr std::array<LexerWorkload, 24> visible{
        LexerWorkload::cpp_visible_60,        LexerWorkload::json_visible_60,
        LexerWorkload::python_visible_60,     LexerWorkload::markdown_visible_60,
        LexerWorkload::markdown_worst_line,   LexerWorkload::javascript_visible_60,
        LexerWorkload::html_visible_60,       LexerWorkload::xml_visible_60,
        LexerWorkload::css_visible_60,        LexerWorkload::yaml_visible_60,
        LexerWorkload::toml_visible_60,       LexerWorkload::gdscript_visible_60,
        LexerWorkload::dosini_visible_60,     LexerWorkload::gdresource_visible_60,
        LexerWorkload::cs_visible_60,         LexerWorkload::rust_visible_60,
        LexerWorkload::go_visible_60,         LexerWorkload::php_visible_60,
        LexerWorkload::sql_visible_60,        LexerWorkload::lua_visible_60,
        LexerWorkload::dockerfile_visible_60, LexerWorkload::powershell_visible_60,
        LexerWorkload::shell_visible_60,      LexerWorkload::cmake_visible_60};
    return std::ranges::find(visible, workload) != visible.end();
}

[[nodiscard]] constexpr std::size_t line_count(LexerWorkload workload)
{
    return is_visible(workload) ? 60U : 200U;
}

[[nodiscard]] constexpr std::span<const std::string_view> templates_of(LexerWorkload workload)
{
    constexpr std::array<std::pair<core::Language, std::span<const std::string_view>>, 22> tables{
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
         {core::Language::go, go_templates},
         {core::Language::php, php_templates},
         {core::Language::sql, sql_templates},
         {core::Language::lua, lua_templates},
         {core::Language::dockerfile, dockerfile_templates},
         {core::Language::powershell, powershell_templates},
         {core::Language::shell, shell_templates},
         {core::Language::cmake, cmake_templates}}};
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

// 各行は「型の行 + 言語の行末コメントの印（JSON・Markdown・HTML・XML・CSS・Ini・Dockerfile は
// 空白・gdresource は ` ;`・C++・JavaScript・C#・Rust・Go・PHP は ` //`・SQL・Lua は ` --`・
// Python・YAML・TOML・GDScript・PowerShell・shell・CMake は ` #`）+
// 詰め物」で 100 バイトにそろえる。詰め物は `-` で、Markdown（フェンスの閉じの行を閉じのまま
// 保つ）と Dockerfile（行末の `\` の続きを保つ・#860）だけ空白。
[[nodiscard]] constexpr std::string_view suffix_of(LexerWorkload workload)
{
    const core::Language language = language_of(workload);
    const bool hashed = language == core::Language::python || language == core::Language::yaml ||
                        language == core::Language::toml || language == core::Language::gdscript ||
                        language == core::Language::powershell ||
                        language == core::Language::shell || language == core::Language::cmake;
    const bool slashed = language == core::Language::cpp ||
                         language == core::Language::javascript || language == core::Language::cs ||
                         language == core::Language::rust || language == core::Language::go ||
                         language == core::Language::php;
    const bool dashed = language == core::Language::sql || language == core::Language::lua;
    if (language == core::Language::gdresource)
    {
        return " ;";
    }
    if (dashed)
    {
        return " --";
    }
    if (hashed)
    {
        return " #";
    }
    return slashed ? " //" : " ";
}

[[nodiscard]] constexpr char padding_of(LexerWorkload workload)
{
    const core::Language language = language_of(workload);
    return language == core::Language::markdown || language == core::Language::dockerfile ? ' '
                                                                                          : '-';
}

// 2 行目の型の後ろで行をまたぐ状態に入るか（C++ と JavaScript と CSS のブロックコメント・Python と
// TOML の 3 連引用符・Markdown のフェンス・HTML のコメント・XML の CDATA・YAML
// のブロックスカラー）。 YAML だけは 3 行目（深い字下げの中身）でもまだ中にいて、4 行目で戻る。
// JSON・Ini・Dockerfile（行の続きは plain の種類のまま）は行をまたぐ種類に入らない。
[[nodiscard]] constexpr bool opened_after(LexerWorkload workload, std::size_t index)
{
    const core::Language language = language_of(workload);
    const std::size_t row = index % template_count;
    const bool single_line = language == core::Language::json ||
                             language == core::Language::dosini ||
                             language == core::Language::dockerfile;
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
    // 入力は文書の 1 行目から読む（遡りの始まりの状態は core の 1 本・#861）。
    core::LexState state = core::lex_start_state(language, true);
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
