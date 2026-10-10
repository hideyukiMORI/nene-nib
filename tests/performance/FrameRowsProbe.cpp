#include "FrameRowsProbe.hpp"

#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::size_t rows_index(FrameRowsWorkload workload)
{
    return static_cast<std::size_t>(workload) -
           static_cast<std::size_t>(FrameRowsWorkload::frame_rows_empty);
}

void prepare_rows(Editing &editing, std::size_t index, const std::string &input)
{
    constexpr std::array<std::size_t, 4> viewports{0U, 30U, 30U, 120U};
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(application::VisibleLines{viewports.at(index)}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\rows.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
}

[[nodiscard]] bool row_values_equal(const application::LineView &line, std::size_t number,
                                    std::string_view text, const core::DisplayLine &display)
{
    return line.number.value == number && line.text == text && line.display.text == display.text &&
           std::ranges::equal(line.display.starts, display.starts) &&
           line.selection == core::no_selection_span() && line.matches.empty() &&
           !line.current_match.has_value();
}

[[nodiscard]] bool row_equals(const application::LineView &line, std::size_t index,
                              std::size_t number)
{
    constexpr std::array<std::size_t, 4> totals{1U, 2U, 120U, 120U};
    if (index == 0U)
    {
        return row_values_equal(line, number, "", core::DisplayLine{"", {0U}});
    }
    if (number == totals.at(index))
    {
        return row_values_equal(line, number, "tail",
                                core::DisplayLine{"tail", {0U, 1U, 2U, 3U, 4U}});
    }
    return row_values_equal(
        line, number, "a日\t🖋\x01 row",
        core::DisplayLine{"a日\t🖋^A row", {0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U, 10U}});
}

[[nodiscard]] bool rows_equal(const application::EditorFrame &frame, std::size_t index)
{
    constexpr std::array<std::size_t, 4> counts{1U, 2U, 30U, 120U};
    constexpr std::array<std::size_t, 4> totals{1U, 2U, 120U, 120U};
    const application::CaretView caret{{core::LineNumber{1U}, core::Column{1U}},
                                       core::CaretShape::bar};
    if (frame.lines.size() != counts.at(index) || frame.first_visible.value != 1U ||
        frame.total_lines != totals.at(index) || frame.caret != caret ||
        frame.mode != core::EditMode::ordinary || frame.vim_mode != core::VimMode::normal ||
        frame.composition.has_value() || frame.command_composition.has_value() ||
        frame.command_line.has_value() || frame.document.last_failure.has_value())
    {
        return false;
    }
    for (std::size_t at = 0; at < frame.lines.size(); ++at)
    {
        if (!row_equals(frame.lines.at(at), index, at + 1U))
        {
            return false;
        }
    }
    return true;
}
} // namespace

std::string frame_rows_input(FrameRowsWorkload workload)
{
    const std::size_t index = rows_index(workload);
    if (index == 0U)
    {
        return "";
    }
    const std::size_t units = index == 1U ? 1U : 119U;
    std::string input;
    for (std::size_t at = 0; at < units; ++at)
    {
        input += "a日\t🖋\x01 row\r\n";
    }
    input += "tail";
    return input;
}

std::expected<std::uint64_t, ProbeFailure>
framed_rows(FrameRowsWorkload workload, const std::string &input, application::TimingPort &timing)
{
    if (input != frame_rows_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t index = rows_index(workload);
    Editing editing;
    prepare_rows(editing, index, input);
    const auto &controller = editing.controller();
    if (!rows_equal(controller.frame(), index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t at = 0; at < 64U; ++at)
    {
        const auto frame = controller.frame();
        checksum += frame.lines.size() + frame.first_visible.value + frame.total_lines +
                    frame.caret.position.line.value + frame.caret.position.column.value;
    }
    timing.mark(core::Milestone::probe_finished);
    constexpr std::array<std::uint64_t, 4> checksums{320U, 448U, 9792U, 15552U};
    if (!rows_equal(controller.frame(), index) || checksum != checksums.at(index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
