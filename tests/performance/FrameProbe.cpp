#include "FrameProbe.hpp"

#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SelectEditMode.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimSearchPattern.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::size_t frame_index(ProbeWorkload workload)
{
    return static_cast<std::size_t>(workload) -
           static_cast<std::size_t>(ProbeWorkload::frame_dense_ascii);
}

[[nodiscard]] std::string repeated_frame_text(std::string_view unit, std::size_t count)
{
    std::string text;
    text.reserve(unit.size() * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        text += unit;
    }
    return text;
}

void place_frame_caret(application::EditorController &controller, std::size_t column,
                       core::SelectionAnchoring anchoring)
{
    static_cast<void>(controller.apply(
        application::PlaceCaret{{core::LineNumber{1U}, core::Column{column}}, anchoring}));
}

void prepare_frame(Editing &editing, std::size_t index, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(application::VisibleLines{1U}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\frame.txt").value()}));
    if (index == 3U)
    {
        static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
        place_frame_caret(controller, 8193U, core::SelectionAnchoring::collapse);
        place_frame_caret(controller, 24577U, core::SelectionAnchoring::extend);
        return;
    }
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(controller.apply(application::VimKeyPress{
        core::VimSearchPattern{"a", core::VimSearchDirection::forward, std::nullopt}}));
    constexpr std::array<std::size_t, 5> columns{1U, 2U, 32768U, 24577U, 8193U};
    place_frame_caret(controller, columns.at(index), core::SelectionAnchoring::collapse);
    if (index == 4U)
    {
        for (const char32_t key : std::u32string_view{U"v16384l"})
        {
            static_cast<void>(controller.apply(application::VimKeyPress{core::VimCharacter{key}}));
        }
    }
}

[[nodiscard]] bool frame_state_equals(const application::EditorFrame &frame, std::size_t index)
{
    constexpr std::array<std::size_t, 5> columns{1U, 2U, 32768U, 24577U, 24577U};
    const core::EditMode mode = index == 3U ? core::EditMode::ordinary : core::EditMode::vim;
    const core::VimMode vim = index == 4U ? core::VimMode::visual : core::VimMode::normal;
    return frame.lines.size() == 1U && frame.first_visible.value == 1U && frame.total_lines == 1U &&
           frame.caret.position.line.value == 1U &&
           frame.caret.position.column.value == columns.at(index) && frame.mode == mode &&
           frame.vim_mode == vim && !frame.command_line.has_value() &&
           !frame.composition.has_value() && !frame.command_composition.has_value() &&
           !frame.document.last_failure.has_value();
}

[[nodiscard]] bool display_equals(const application::LineView &line, std::size_t index,
                                  const std::string &input)
{
    const std::string expected = index == 1U ? repeated_frame_text("日a\t🖋^A ", 4096U) : input;
    const std::size_t size = index == 1U ? 24577U : 32769U;
    if (line.number.value != 1U || line.text != input || line.display.text != expected ||
        line.display.starts.size() != size)
    {
        return false;
    }
    constexpr std::array<std::size_t, 6> mixed{0U, 1U, 2U, 3U, 4U, 6U};
    for (std::size_t at = 0; at < size; ++at)
    {
        const std::size_t position =
            index == 1U ? (at == 24576U ? 28672U : 7U * (at / 6U) + mixed.at(at % 6U)) : at;
        if (line.display.starts.at(at) != position)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool matches_equal(const application::LineView &line, std::size_t index)
{
    constexpr std::array<std::size_t, 5> counts{8192U, 4096U, 1U, 0U, 8192U};
    constexpr std::array<std::size_t, 5> strides{4U, 6U, 0U, 0U, 4U};
    constexpr std::array<std::size_t, 5> bases{1U, 2U, 32768U, 0U, 1U};
    if (line.matches.size() != counts.at(index))
    {
        return false;
    }
    for (std::size_t at = 0; at < counts.at(index); ++at)
    {
        const std::size_t begin = bases.at(index) + strides.at(index) * at;
        if (line.matches.at(at) != core::SelectionSpan{core::SelectionPresence::present,
                                                       core::Column{begin},
                                                       core::Column{begin + 1U}})
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] core::SelectionSpan expected_frame_selection(std::size_t index)
{
    if (index < 3U)
    {
        return core::no_selection_span();
    }
    return core::SelectionSpan{core::SelectionPresence::present, core::Column{8193U},
                               core::Column{index == 3U ? 24577U : 24578U}};
}

[[nodiscard]] bool current_match_equals(const application::LineView &line, std::size_t index)
{
    if (index == 3U)
    {
        return !line.current_match.has_value();
    }
    if (!line.current_match.has_value())
    {
        return false;
    }
    constexpr std::array<std::size_t, 5> begins{1U, 2U, 32768U, 0U, 24577U};
    const std::size_t begin = begins.at(index);
    return line.current_match.value() == core::SelectionSpan{core::SelectionPresence::present,
                                                             core::Column{begin},
                                                             core::Column{begin + 1U}};
}

[[nodiscard]] std::uint64_t frame_checksum(const std::string &input,
                                           const application::EditorFrame &frame)
{
    const auto &line = frame.lines.at(0);
    std::uint64_t checksum = checksum_of(input) + checksum_of(line.display.text) +
                             line.number.value + frame.first_visible.value + frame.total_lines +
                             frame.lines.size() + frame.caret.position.line.value +
                             frame.caret.position.column.value + line.matches.size();
    for (const auto start : line.display.starts)
    {
        checksum += start;
    }
    for (const auto &match : line.matches)
    {
        checksum += match.begin.value + match.end.value;
    }
    if (line.selection.presence == core::SelectionPresence::present)
    {
        checksum += 1U + line.selection.begin.value + line.selection.end.value;
    }
    if (line.current_match.has_value())
    {
        checksum +=
            1U + line.current_match.value().begin.value + line.current_match.value().end.value;
    }
    return checksum;
}
} // namespace

std::string frame_probe_input(ProbeWorkload workload)
{
    const std::size_t index = frame_index(workload);
    if (index == 1U)
    {
        return repeated_frame_text("日a\t🖋\x01 ", 4096U);
    }
    if (index == 2U)
    {
        return std::string(32767U, 'x') + "a";
    }
    return repeated_frame_text("a x ", 8192U);
}

std::expected<std::uint64_t, ProbeFailure>
framed_line(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    if (input != frame_probe_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t index = frame_index(workload);
    Editing editing;
    prepare_frame(editing, index, input);
    timing.mark(core::Milestone::probe_started);
    const auto frame = editing.controller().frame();
    timing.mark(core::Milestone::probe_finished);
    if (!frame_state_equals(frame, index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &line = frame.lines.at(0);
    if (!display_equals(line, index, input) || !matches_equal(line, index) ||
        line.selection != expected_frame_selection(index) || !current_match_equals(line, index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return frame_checksum(input, frame);
}
} // namespace nenenib::tests::performance
