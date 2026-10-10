#include "SearchSnapshotProbe.hpp"

#include "CommandText.hpp"
#include "Editing.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SaveDocument.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimSearchPattern.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstdio>
#include <optional>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::size_t snapshot_index(SearchSnapshotWorkload workload)
{
    return static_cast<std::size_t>(workload);
}

[[nodiscard]] bool is_insertion(std::size_t index)
{
    return index >= 3U && index <= 5U;
}

[[nodiscard]] core::FilePath snapshot_path()
{
    return core::FilePath::parse("C:\\nib-probe\\search-snapshot.txt").value();
}

[[nodiscard]] std::string snapshot_body(std::size_t index)
{
    constexpr std::array<std::string_view, 8> bodies{
        "a x\r\na x\r\na x", "x", "", "", "", "", "a x a", "a x\r\na x\r\na x"};
    if (index == 2U)
    {
        return std::string(256U, 'a') + " x " + std::string(256U, 'a');
    }
    return std::string(bodies.at(index));
}

void snapshot_key(application::EditorController &controller, char32_t key)
{
    static_cast<void>(controller.apply(application::VimKeyPress{core::VimCharacter{key}}));
}

void prepare_snapshot(Editing &editing, std::size_t index, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold(snapshot_body(index));
    const std::size_t rows = index == 0U || index == 7U ? 3U : 1U;
    static_cast<void>(controller.apply(application::VisibleLines{rows}));
    static_cast<void>(controller.apply(application::OpenDocument{snapshot_path()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    if (index == 7U)
    {
        snapshot_key(controller, U'/');
        static_cast<void>(controller.apply(application::CommandText{input}));
        return;
    }
    if (index != 5U && index != 6U)
    {
        static_cast<void>(controller.apply(application::VimKeyPress{
            core::VimSearchPattern{input, core::VimSearchDirection::forward, std::nullopt}}));
        snapshot_key(controller, U'h');
    }
    static_cast<void>(controller.apply(application::PlaceCaret{
        {core::LineNumber{1U}, core::Column{1U}}, core::SelectionAnchoring::collapse}));
    if (is_insertion(index))
    {
        static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
    }
}

[[nodiscard]] core::SelectionSpan snapshot_span(std::size_t begin, std::size_t end)
{
    return {core::SelectionPresence::present, core::Column{begin}, core::Column{end}};
}

[[nodiscard]] std::vector<core::SelectionSpan> snapshot_matches(std::size_t index, bool finished)
{
    if (index == 0U || index == 7U)
    {
        return {snapshot_span(1U, 2U)};
    }
    if (index == 2U)
    {
        return {snapshot_span(1U, 257U), snapshot_span(260U, 516U)};
    }
    if (index == 6U && finished)
    {
        return {snapshot_span(1U, 2U), snapshot_span(5U, 6U)};
    }
    return {};
}

[[nodiscard]] bool snapshot_line_equal(const application::LineView &line, std::size_t index,
                                       bool finished)
{
    const bool three_rows = index == 0U || index == 7U;
    const std::string body = is_insertion(index) && finished ? std::string(200U, 'x')
                             : three_rows                    ? "a x"
                                                             : snapshot_body(index);
    const auto matches = snapshot_matches(index, finished);
    const std::size_t current = index == 7U ? 2U : 1U;
    const auto expected = !matches.empty() && line.number.value == current
                              ? std::optional{matches.front()}
                              : std::nullopt;
    if (line.text != body || line.display.text != body ||
        line.display.starts.size() != body.size() + 1U ||
        line.selection != core::no_selection_span() || line.matches != matches ||
        line.current_match != expected)
    {
        return false;
    }
    for (std::size_t at = 0; at < line.display.starts.size(); ++at)
    {
        if (line.display.starts.at(at) != at)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool snapshot_input_equal(const application::EditorFrame &frame, std::size_t index)
{
    if (index != 7U)
    {
        return !frame.command_line.has_value();
    }
    if (!frame.command_line.has_value())
    {
        return false;
    }
    const auto &line = frame.command_line.value();
    return line.prompt == core::InputLinePrompt::search_forward && line.text == "a" &&
           line.caret.value == 1U && line.completions.empty() && !line.completion_index.has_value();
}

[[nodiscard]] bool snapshot_message_equal(const application::EditorFrame &frame, std::size_t index,
                                          bool finished)
{
    if (index == 6U && finished)
    {
        return frame.command_message.has_value() &&
               frame.command_message.value().text() == "search hit BOTTOM, continuing at TOP";
    }
    return !frame.command_message.has_value();
}

[[nodiscard]] bool snapshot_layout_equal(const application::EditorFrame &frame, std::size_t index,
                                         bool finished)
{
    const std::size_t rows = index == 0U || index == 7U ? 3U : 1U;
    const std::size_t column = is_insertion(index) && finished ? 201U : 1U;
    const core::EditMode mode =
        is_insertion(index) ? core::EditMode::ordinary : core::EditMode::vim;
    return frame.lines.size() == rows && frame.first_visible.value == 1U &&
           frame.total_lines == rows && frame.caret.position.line.value == 1U &&
           frame.caret.position.column.value == column && frame.mode == mode;
}

[[nodiscard]] bool snapshot_frame_equal(const application::EditorFrame &frame, std::size_t index,
                                        bool finished)
{
    if (!snapshot_layout_equal(frame, index, finished) || frame.vim_mode != core::VimMode::normal ||
        frame.composition.has_value() || frame.command_composition.has_value() ||
        frame.document.last_failure.has_value() || !snapshot_input_equal(frame, index) ||
        !snapshot_message_equal(frame, index, finished))
    {
        return false;
    }
    for (std::size_t at = 0; at < frame.lines.size(); ++at)
    {
        if (frame.lines.at(at).number.value != at + 1U ||
            !snapshot_line_equal(frame.lines.at(at), index, finished))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool snapshot_state_equal(Editing &editing, std::size_t index, bool finished)
{
    const bool remembered = index != 5U && index != 7U && (index != 6U || finished);
    const auto &controller = editing.controller();
    return controller.vim_state().last_search.has_value() == remembered &&
           snapshot_frame_equal(controller.frame(), index, finished);
}

[[nodiscard]] bool snapshot_saved_equal(Editing &editing, std::size_t index, bool finished)
{
    const auto delivered = editing.controller().apply(
        application::SaveDocument{snapshot_path(), core::TextEncoding::utf8});
    const std::string expected =
        is_insertion(index) && finished ? std::string(200U, 'x') : snapshot_body(index);
    return !delivered.document.last_failure.has_value() &&
           delivered.document.save_state == core::SaveState::saved &&
           editing.files().written_path() == snapshot_path().text() &&
           editing.files().written() == expected;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure> snapshot_failed(const char *stage)
{
    std::fprintf(stderr, "search snapshot probe failed: %s\n", stage);
    return std::unexpected(ProbeFailure::wrong_result);
}

[[nodiscard]] std::uint64_t snapshot_frames(const application::EditorController &controller,
                                            application::TimingPort &timing)
{
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t at = 0; at < 64U; ++at)
    {
        const auto frame = controller.frame();
        checksum += frame.lines.size() + frame.first_visible.value + frame.total_lines +
                    frame.caret.position.line.value + frame.caret.position.column.value;
    }
    timing.mark(core::Milestone::probe_finished);
    return checksum;
}

[[nodiscard]] bool snapshot_operations(Editing &editing, std::size_t index,
                                       const std::string &input, application::TimingPort &timing)
{
    const application::EditorIntent first =
        index == 2U ? application::EditorIntent{application::VimKeyPress{core::VimCharacter{U'n'}}}
        : index == 6U ? application::EditorIntent{application::VimKeyPress{core::VimSearchPattern{
                            input, core::VimSearchDirection::forward, std::nullopt}}}
                      : application::EditorIntent{application::InsertText{"x"}};
    const application::EditorIntent second =
        index == 2U ? application::EditorIntent{application::VimKeyPress{core::VimCharacter{U'N'}}}
                    : first;
    const std::size_t count = is_insertion(index) ? 200U : 64U;
    std::optional<application::EditorDelivery> delivered;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t at = 0; at < count; ++at)
    {
        delivered = editing.controller().apply(at % 2U == 0U ? first : second);
    }
    timing.mark(core::Milestone::probe_finished);
    return delivered.has_value() && !delivered.value().document.last_failure.has_value();
}

[[nodiscard]] bool maximum_query_is_reachable(const std::string &input)
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    snapshot_key(controller, U'/');
    static_cast<void>(controller.apply(application::CommandText{input}));
    const auto frame = controller.frame();
    if (!frame.command_line.has_value() || frame.command_line.value().text != input ||
        frame.command_line.value().caret.value != input.size())
    {
        return false;
    }
    static_cast<void>(controller.apply(application::SubmitCommand{}));
    return !controller.command_line_active() && controller.vim_state().last_search.has_value();
}

[[nodiscard]] bool measured_snapshot(Editing &editing, std::size_t index, const std::string &input,
                                     application::TimingPort &timing)
{
    if (index == 0U || index == 1U || index == 7U)
    {
        const std::uint64_t expected = index == 1U ? 320U : 576U;
        return snapshot_frames(editing.controller(), timing) == expected;
    }
    return snapshot_operations(editing, index, input, timing);
}
} // namespace

std::string search_snapshot_input(SearchSnapshotWorkload workload)
{
    const std::size_t index = snapshot_index(workload);
    if (index == 5U)
    {
        return "";
    }
    return std::string(index >= 1U && index <= 3U ? 256U : 1U, 'a');
}

std::expected<std::uint64_t, ProbeFailure> probed_search_snapshot(SearchSnapshotWorkload workload,
                                                                  const std::string &input,
                                                                  application::TimingPort &timing)
{
    if (input != search_snapshot_input(workload))
    {
        return snapshot_failed("fixed input");
    }
    const std::size_t index = snapshot_index(workload);
    if (index >= 1U && index <= 3U && !maximum_query_is_reachable(input))
    {
        return snapshot_failed("normal search input reachability");
    }
    Editing editing;
    prepare_snapshot(editing, index, input);
    if (!snapshot_state_equal(editing, index, false) ||
        !snapshot_saved_equal(editing, index, false))
    {
        return snapshot_failed("before values and whole body");
    }
    if (!measured_snapshot(editing, index, input, timing))
    {
        return snapshot_failed("timed checksum or delivery");
    }
    if (!snapshot_state_equal(editing, index, true) || !snapshot_saved_equal(editing, index, true))
    {
        return snapshot_failed("after values and whole body");
    }
    return checksum_of(editing.files().written()) ^ checksum_of(input);
}
} // namespace nenenib::tests::performance
