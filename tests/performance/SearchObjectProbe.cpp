#include "SearchObjectProbe.hpp"

#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimPatternCase.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchSnapshot.hpp"
#include "VimSmartcase.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t line_bytes = std::size_t{1024} * 1024U;
constexpr std::string_view unit = "ab ";
constexpr std::size_t workloads = 8U;
// 鍵の列（1 歩ずつ流す）。`n` `N` は 1 鍵、`gn` は 2 鍵、`dgn` `dgN` は 3 鍵。
constexpr std::array<std::string_view, workloads> keys{"n", "gn", "dgn", "n",
                                                       "N", "gn", "dgn", "dgN"};
constexpr std::array<bool, workloads> at_end{false, false, false, true, true, true, true, true};
// 本文を変える鍵か（`dgn` `dgN`）。ほかは選択か移動だけ。
constexpr std::array<bool, workloads> edits{false, false, true, false, false, false, true, true};

// `ab ` の繰り返しで 1 MiB に足りない分を埋め、最後は `ab` で終える（最後の文字は当たりの中）。
[[nodiscard]] std::string search_line()
{
    std::string text;
    text.reserve(line_bytes);
    while (text.size() + unit.size() + 2U <= line_bytes)
    {
        text += unit;
    }
    text += "ab";
    return text;
}
} // namespace

std::string search_object_input(SearchObjectWorkload workload)
{
    static_cast<void>(workload);
    return search_line();
}

std::expected<std::uint64_t, ProbeFailure> search_object_keys(SearchObjectWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    const auto index = static_cast<std::size_t>(workload);
    if (!(index < workloads) || !parsed.has_value() || input != search_object_input(workload) ||
        parsed.value().line_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::TextBuffer &text = parsed.value();
    const core::Selection selection =
        core::collapsed_at(core::Offset{at_end.at(index) ? input.size() - 1U : 0U});
    core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized});
    state.last_search =
        core::VimSearchSnapshot::from("ab", core::VimSearchDirection::forward,
                                      core::VimPatternCase::sensitive, core::VimSmartcase::ignored);
    const std::string_view typed = keys.at(index);
    const core::VimEditorView view{text, selection, std::nullopt};
    std::cerr << "preparedPieces=" << text.piece_count() << " preparedHash=" << checksum_of(input)
              << '\n';
    timing.mark(core::Milestone::probe_started);
    core::VimStep step{state, core::VimNoEffect{}};
    for (const char key : typed)
    {
        step = core::vim_step(step.next, view, core::VimCharacter{static_cast<char32_t>(key)});
    }
    timing.mark(core::Milestone::probe_finished);
    const bool selected = std::holds_alternative<core::VimSelect>(step.effect);
    const bool changed = !std::holds_alternative<core::VimNoEffect>(step.effect) && !selected;
    if (step.failure.has_value() || (edits.at(index) ? !changed : !(selected || changed)))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "effect=" << step.effect.index() << '\n';
    return checksum_of(input) + step.effect.index();
}
} // namespace nenenib::tests::performance
