#include "LexerProbe.hpp"

#include "Language.hpp"
#include "LexKind.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"
#include "Lexer.hpp"
#include "Milestone.hpp"
#include "ProbeWorkload.hpp"

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
        return core::Language::markdown;
    case LexerWorkload::javascript_visible_60:
    case LexerWorkload::javascript_backfill_200:
        return core::Language::javascript;
    }
    std::unreachable();
}

[[nodiscard]] constexpr bool is_visible(LexerWorkload workload)
{
    return workload == LexerWorkload::cpp_visible_60 ||
           workload == LexerWorkload::json_visible_60 ||
           workload == LexerWorkload::python_visible_60 ||
           workload == LexerWorkload::markdown_visible_60 ||
           workload == LexerWorkload::javascript_visible_60;
}

[[nodiscard]] constexpr std::size_t line_count(LexerWorkload workload)
{
    return is_visible(workload) ? 60U : 200U;
}

[[nodiscard]] constexpr std::span<const std::string_view> templates_of(LexerWorkload workload)
{
    const core::Language language = language_of(workload);
    if (language == core::Language::json)
    {
        return json_templates;
    }
    if (language == core::Language::markdown)
    {
        return markdown_templates;
    }
    if (language == core::Language::javascript)
    {
        return javascript_templates;
    }
    return language == core::Language::python ? std::span{python_templates}
                                              : std::span{cpp_templates};
}

// 各行は「型の行 + 言語の行末コメントの印（JSON と Markdown は空白）+ 詰め物」で 100 バイトに
// そろえる。詰め物は `-` で、Markdown だけ空白（フェンスの閉じの行を閉じのまま保つ）。
[[nodiscard]] constexpr std::string_view suffix_of(LexerWorkload workload)
{
    const core::Language language = language_of(workload);
    if (language == core::Language::json || language == core::Language::markdown)
    {
        return " ";
    }
    return language == core::Language::python ? " #" : " //";
}

[[nodiscard]] constexpr char padding_of(LexerWorkload workload)
{
    return language_of(workload) == core::Language::markdown ? ' ' : '-';
}

// 2 行目の型の後ろで行をまたぐ状態に入るか（C++ と JavaScript のブロックコメント・Python の 3 連
// 引用符・Markdown のフェンス）。
[[nodiscard]] constexpr bool opened_after(LexerWorkload workload, std::size_t index)
{
    return language_of(workload) != core::Language::json && index % template_count == 1U;
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
