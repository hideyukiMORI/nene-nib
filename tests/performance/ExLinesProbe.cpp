#include "ExLinesProbe.hpp"

#include "ExAddress.hpp"
#include "ExGlobalRequest.hpp"
#include "ExLineVerb.hpp"
#include "ExLinesRequest.hpp"
#include "ExRange.hpp"
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

[[nodiscard]] core::ExLinesRequest delete_request(std::optional<core::ExRange> range)
{
    return core::ExLinesRequest{std::move(range), core::ExLineVerb::delete_lines, std::nullopt,
                                std::nullopt};
}

[[nodiscard]] core::VimStep stepped(ExLinesWorkload workload, const core::TextBuffer &text)
{
    const core::Selection selection = core::collapsed_at(core::Offset{0});
    const core::VimState state = core::vim_resting_state(
        core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized});
    const core::VimEditorView view{text, selection, std::nullopt};
    switch (workload)
    {
    case ExLinesWorkload::global_delete_half_100k:
        return core::vim_global(
            state, view, core::ExGlobalRequest{std::nullopt, false, '/', "x", delete_request({})});
    case ExLinesWorkload::delete_all_100k:
        return core::vim_lines_edited(
            state, view,
            delete_request(
                core::ExRange{{core::ExAddress{core::ExAddressBase::number, 1, '\0', 0, false},
                               core::ExAddress{core::ExAddressBase::last, 0, '\0', 0, false}}}));
    }
    std::unreachable();
}

// 行の塊の編集を 1 つの置き換えに畳んで本文に当てる（controller の apply_block と同じ形・LF
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

// 期待の行数（`:g/x/d` は y の行の 5 万行・`:%d` は空の 1 行）と、塊の数。
[[nodiscard]] bool expected_result(ExLinesWorkload workload, const core::TextBuffer &after,
                                   std::size_t edits)
{
    switch (workload)
    {
    case ExLinesWorkload::global_delete_half_100k:
        return after.line_count() == probe_lines / 2 && edits == probe_lines / 2;
    case ExLinesWorkload::delete_all_100k:
        return after.size_bytes() == 0 && edits == 1;
    }
    std::unreachable();
}
} // namespace

std::string ex_lines_input(ExLinesWorkload workload)
{
    static_cast<void>(workload);
    std::string text;
    for (std::size_t line = 1; line <= probe_lines; ++line)
    {
        const std::string number = std::to_string(line);
        text += "line" + std::string(6 - number.size(), '0') + number;
        text += line % 2 == 1 ? " x" : " y";
        if (line < probe_lines)
        {
            text += '\n';
        }
    }
    return text;
}

std::expected<std::uint64_t, ProbeFailure>
ex_lines_run(ExLinesWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value() || input != ex_lines_input(workload) ||
        parsed.value().line_count() != probe_lines)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::TextBuffer &text = parsed.value();
    timing.mark(core::Milestone::probe_started);
    const core::VimStep step = stepped(workload, text);
    const auto *block = std::get_if<core::VimReplaceBlock>(&step.effect);
    const core::TextBuffer after = block != nullptr ? applied(text, block->edits) : text;
    timing.mark(core::Milestone::probe_finished);
    if (block == nullptr || !expected_result(workload, after, block->edits.size()))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "edits=" << block->edits.size() << '\n';
    return checksum_of(input) + after.size_bytes();
}
} // namespace nenenib::tests::performance
