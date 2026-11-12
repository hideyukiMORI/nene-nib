#include "SubstituteProbe.hpp"

#include "ExAddress.hpp"
#include "ExRange.hpp"
#include "ExSubstituteRequest.hpp"
#include "Milestone.hpp"
#include "ProbeWorkload.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimEditorView.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimReplaceBlock.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t probe_lines = 100000;
constexpr std::string_view line_tail = " a b a";

[[nodiscard]] core::ExSubstituteRequest percent_request(std::string pattern)
{
    core::ExRange all{{core::ExAddress{core::ExAddressBase::number, 1, '\0', 0, false},
                       core::ExAddress{core::ExAddressBase::last, 0, '\0', 0, false}}};
    return core::ExSubstituteRequest{
        std::move(all), core::ExSubstituteSource::typed, '/', std::move(pattern), "b", false, "g",
        std::nullopt};
}

// 行ごとの編集を 1 つの置き換えに畳んで本文に当てる（controller の apply_block と同じ形・LF
// の本文）。
[[nodiscard]] core::TextBuffer applied(const core::TextBuffer &text,
                                       const std::vector<core::VimBlockEdit> &edits)
{
    const core::Offset begin = edits.front().range.begin;
    std::string body;
    core::Offset at = begin;
    for (const core::VimBlockEdit &edit : edits)
    {
        body += text.text_range(at, edit.range.begin);
        body += edit.utf8;
        at = edit.range.end;
    }
    return text.replaced(begin, edits.back().range.end, body);
}
} // namespace

std::string substitute_input(SubstituteWorkload workload)
{
    static_cast<void>(workload);
    std::string text;
    for (std::size_t line = 1; line <= probe_lines; ++line)
    {
        const std::string number = std::to_string(line);
        text += "line" + std::string(6 - number.size(), '0') + number;
        text += line_tail;
        if (line < probe_lines)
        {
            text += '\n';
        }
    }
    return text;
}

std::expected<std::uint64_t, ProbeFailure> substitute_lines(SubstituteWorkload workload,
                                                            const std::string &input,
                                                            application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input != substitute_input(workload) ||
        parsed.value().line_count() != probe_lines)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::TextBuffer &text = parsed.value();
    const core::Selection selection = core::collapsed_at(core::Offset{0});
    const core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized});
    const bool every = workload == SubstituteWorkload::all_lines_100k;
    timing.mark(core::Milestone::probe_started);
    const core::VimStep step =
        core::vim_substituted(state, core::VimEditorView{text, selection, std::nullopt},
                              percent_request(every ? "a" : "zzz"));
    const auto *block = std::get_if<core::VimReplaceBlock>(&step.effect);
    const core::TextBuffer after = block != nullptr ? applied(text, block->edits) : text;
    timing.mark(core::Milestone::probe_finished);
    if ((every && (block == nullptr || block->edits.size() != probe_lines)) ||
        (!every && block != nullptr) || after.size_bytes() != input.size())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "edits=" << (block != nullptr ? block->edits.size() : 0U) << '\n';
    return checksum_of(input) + after.size_bytes();
}
} // namespace nenenib::tests::performance
