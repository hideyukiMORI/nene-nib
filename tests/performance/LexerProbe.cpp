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
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t line_bytes = 100U;
// 2 行目がブロックコメントを開き 3 行目が閉じる。raw string・文字列・数字・演算子・前処理を含む。
// 入力は eng/compare-probes.py の lexer_input と同じ生成の形。
constexpr std::array<std::string_view, 6> templates{
    "#include <vector> // container",
    "static constexpr std::uint32_t mask = 0xFF'FFu; /* block comment begins",
    "   continues the block comment */ int value = compute(12.5e-3f, 'x');",
    "auto text = R\"raw(raw string with ) inside)raw\"; if (text.size() >= 10) { return; }",
    "for (int i = 0; i < count; ++i) { total += values[i] * 2; } // loop",
    "const char *name = \"string with \\\" escape\"; bool ok = name != nullptr && true;"};

[[nodiscard]] constexpr std::size_t line_count(LexerWorkload workload)
{
    return workload == LexerWorkload::cpp_visible_60 ? 60U : 200U;
}

// 各行は「型の行 + ` //` + `-` の詰め物」で 100 バイトにそろえる。
[[nodiscard]] std::string padded(std::string_view line)
{
    std::string text{line};
    text += " //";
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

// 行ごとの区間の数と終わりの桁、出る状態の種類を足す。2 行目の後はブロックコメントの中。
[[nodiscard]] std::uint64_t lexed_sum(const core::LexLine &lexed, std::size_t index, bool &valid)
{
    const bool opens = index % templates.size() == 1U;
    valid = valid && (lexed.out.kind == core::LexKind::block_comment) == opens;
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
    std::string text;
    text.reserve(line_count(workload) * (line_bytes + 1U));
    for (std::size_t index = 0; index < line_count(workload); ++index)
    {
        text += padded(templates.at(index % templates.size()));
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
    const bool visible = workload == LexerWorkload::cpp_visible_60;
    std::cerr << "preparedBytes=" << input.size() << " lines=" << lines.size() << '\n';
    core::LexState state{};
    std::uint64_t checksum = checksum_of(input);
    bool valid = true;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; visible && index < lines.size(); ++index)
    {
        const core::LexLine lexed = core::lex_line(core::Language::cpp, state, lines[index]);
        checksum += lexed_sum(lexed, index, valid);
        state = lexed.out;
    }
    for (std::size_t index = 0; !visible && index < lines.size(); ++index)
    {
        state = core::lex_state(core::Language::cpp, state, lines[index]);
        checksum += static_cast<std::uint64_t>(state.kind);
    }
    timing.mark(core::Milestone::probe_finished);
    // 最後の行が 2 行目の型ならブロックコメントの中で終わる（200 行は 199 % 6 == 1）。
    const bool open_at_end = (lines.size() - 1U) % templates.size() == 1U;
    if (!valid || (state.kind == core::LexKind::block_comment) != open_at_end)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
