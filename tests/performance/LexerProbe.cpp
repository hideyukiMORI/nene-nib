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
    }
    std::unreachable();
}

[[nodiscard]] constexpr bool is_visible(LexerWorkload workload)
{
    return workload == LexerWorkload::cpp_visible_60 ||
           workload == LexerWorkload::json_visible_60 ||
           workload == LexerWorkload::python_visible_60;
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
    return language == core::Language::python ? std::span{python_templates}
                                              : std::span{cpp_templates};
}

// 各行は「型の行 + 言語の行末コメントの印（JSON は空白）+ `-` の詰め物」で 100 バイトにそろえる。
[[nodiscard]] constexpr std::string_view suffix_of(LexerWorkload workload)
{
    const core::Language language = language_of(workload);
    if (language == core::Language::json)
    {
        return " ";
    }
    return language == core::Language::python ? " #" : " //";
}

// 2 行目の型の後ろで行をまたぐ状態に入るか（C++ のブロックコメント・Python の 3 連引用符）。
[[nodiscard]] constexpr bool opened_after(LexerWorkload workload, std::size_t index)
{
    return language_of(workload) != core::Language::json && index % template_count == 1U;
}

[[nodiscard]] std::string padded(std::string_view line, std::string_view suffix)
{
    std::string text{line};
    text += suffix;
    text.append(line_bytes - text.size(), '-');
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
        text += padded(templates[index % templates.size()], suffix_of(workload));
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
