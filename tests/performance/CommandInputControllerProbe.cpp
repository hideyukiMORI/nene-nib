#include "CommandInputControllerProbe.hpp"

#include "CancelCommand.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "Milestone.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
struct ControllerCase
{
    core::InputLinePrompt prompt;
    application::EditorIntent opening;
    bool searches;
    std::optional<std::size_t> palette_count;
    std::span<const application::EditorIntent> cycle;
    std::uint64_t checksum;
    std::size_t new_requests;
    bool stages_fill;
};

const std::array<application::EditorIntent, 2> edit_cycle{
    application::CommandText{"b"}, application::EditCommand{core::CommandEdit::backspace}};
const std::array<application::EditorIntent, 4> fill_cycle{
    application::SubmitCommand{}, application::CancelCommand{}, application::OpenCommandPalette{},
    application::CommandText{":fz"}};
const std::array<ControllerCase, 4> controller_cases{{
    {core::InputLinePrompt::search_forward, application::VimKeyPress{core::VimCharacter{U'/'}},
     true, std::nullopt, edit_cycle, 1152U, 0U, false},
    {core::InputLinePrompt::ex, application::VimKeyPress{core::VimCharacter{U':'}}, false,
     std::nullopt, edit_cycle, 1152U, 0U, false},
    {core::InputLinePrompt::palette, application::OpenCommandPalette{}, false, 0U, edit_cycle,
     1152U, 0U, false},
    {core::InputLinePrompt::palette, application::OpenCommandPalette{}, false, 1U, fill_cycle,
     2304U, 64U, true},
}};

[[nodiscard]] const ControllerCase &controller_spec(CommandInputWorkload workload)
{
    switch (workload)
    {
    case CommandInputWorkload::controller_search:
        return controller_cases.at(0U);
    case CommandInputWorkload::controller_ex:
        return controller_cases.at(1U);
    case CommandInputWorkload::controller_palette:
        return controller_cases.at(2U);
    case CommandInputWorkload::controller_fill:
        return controller_cases.at(3U);
    case CommandInputWorkload::search_long:
    case CommandInputWorkload::ex_long:
    case CommandInputWorkload::palette_long:
    case CommandInputWorkload::search_short:
    case CommandInputWorkload::ex_short:
    case CommandInputWorkload::palette_short:
    case CommandInputWorkload::ex_left_long:
    case CommandInputWorkload::palette_fill_long:
        break;
    }
    std::unreachable();
}

[[nodiscard]] core::SelectionSpan match_span(std::size_t begin, std::size_t end)
{
    return {core::SelectionPresence::present, core::Column{begin}, core::Column{end}};
}

[[nodiscard]] bool body_equal(const application::EditorFrame &frame, const ControllerCase &sample)
{
    if (frame.lines.size() != 1U)
    {
        return false;
    }
    const auto &line = frame.lines.at(0U);
    const std::vector<core::SelectionSpan> matches =
        sample.searches ? std::vector{match_span(1U, 2U), match_span(5U, 6U)}
                        : std::vector<core::SelectionSpan>{};
    const std::optional<core::SelectionSpan> current =
        sample.searches ? std::optional{match_span(5U, 6U)} : std::nullopt;
    return line.number.value == 1U && line.text == "a x a" && line.display.text == "a x a" &&
           line.display.starts == std::vector<std::size_t>{0U, 1U, 2U, 3U, 4U, 5U} &&
           line.selection == core::no_selection_span() && line.matches == matches &&
           line.current_match == current && frame.caret.position.line.value == 1U &&
           frame.caret.position.column.value == 1U && frame.first_visible.value == 1U &&
           frame.total_lines == 1U && frame.mode == core::EditMode::vim &&
           frame.vim_mode == core::VimMode::normal;
}

[[nodiscard]] bool document_equal(const application::DocumentView &document)
{
    return document.title.text() == "probe.txt" && document.title.code_point_count() == 9U &&
           document.path.has_value() &&
           document.path.value().text() == "C:\\nib-probe\\probe.txt" &&
           document.save_state == core::SaveState::saved &&
           document.encoding == core::TextEncoding::utf8 && !document.last_failure.has_value();
}

[[nodiscard]] bool palette_equal(const application::EditorFrame &frame,
                                 const ControllerCase &sample)
{
    if (!sample.palette_count.has_value())
    {
        return !frame.command_palette.has_value();
    }
    if (!frame.command_palette.has_value())
    {
        return false;
    }
    const auto &palette = frame.command_palette.value();
    if (sample.palette_count.value() == 0U)
    {
        return palette.rows.empty() && palette.total == 0U && palette.selected == 0U &&
               palette.first == 0U;
    }
    if (palette.rows.size() != 1U || palette.total != 1U || palette.selected != 0U ||
        palette.first != 0U)
    {
        return false;
    }
    const auto &choice = palette.rows.at(0U);
    return choice.kind == core::CommandChoiceKind::fill && choice.command == "set fontsize=" &&
           choice.label.text() == "set fontsize=";
}

[[nodiscard]] bool frame_equal(const application::EditorFrame &frame, const ControllerCase &sample,
                               std::string_view input)
{
    if (!frame.command_line.has_value())
    {
        return false;
    }
    const auto &line = frame.command_line.value();
    return body_equal(frame, sample) && document_equal(frame.document) && frame.tabs.size() == 1U &&
           document_equal(frame.tabs.at(0U)) && frame.active_tab == 0U && line.text == input &&
           line.caret.value == input.size() && core::code_point_count(line.text) == input.size() &&
           line.prompt == sample.prompt && !line.completion_index.has_value() &&
           line.completions.empty() && palette_equal(frame, sample) &&
           !frame.composition.has_value() && !frame.command_composition.has_value() &&
           !frame.command_message.has_value() && !frame.settings_failure.has_value() &&
           core::same_settings(frame.settings, core::default_editor_settings()) &&
           frame.status_items.at(0U).text() == "行 1, 桁 1" &&
           frame.status_items.at(1U).text() == "UTF-8" &&
           frame.status_items.at(2U).text() == "CRLF";
}

void prepare_controller(Editing &editing, const ControllerCase &sample, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold("a x a");
    static_cast<void>(controller.apply(application::VisibleLines{1U}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\probe.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(controller.apply(sample.opening));
    static_cast<void>(controller.apply(application::CommandText{input}));
}

[[nodiscard]] bool fill_is_staged(Editing &editing, const ControllerCase &sample,
                                  const std::string &input)
{
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(application::SubmitCommand{}));
    const auto filled = controller.frame();
    if (!frame_equal(filled, sample, ":set fontsize=") || editing.settings().writes() != 0U)
    {
        return false;
    }
    static_cast<void>(controller.apply(application::CancelCommand{}));
    static_cast<void>(controller.apply(application::OpenCommandPalette{}));
    static_cast<void>(controller.apply(application::CommandText{input}));
    return frame_equal(controller.frame(), sample, input);
}

[[nodiscard]] std::uint64_t cycled_controller(application::EditorController &controller,
                                              const ControllerCase &sample,
                                              application::TimingPort &timing)
{
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t cycle = 0; cycle < 64U; ++cycle)
    {
        for (const auto &intent : sample.cycle)
        {
            const auto delivery = controller.apply(intent);
            checksum += delivery.document.title.text().size();
        }
    }
    timing.mark(core::Milestone::probe_finished);
    return checksum;
}

[[nodiscard]] bool controller_equal(Editing &editing, const ControllerCase &sample,
                                    const std::string &input)
{
    auto &controller = editing.controller();
    const auto documents = controller.documents();
    return frame_equal(controller.frame(), sample, input) && documents.size() == 1U &&
           document_equal(documents.at(0U)) && document_equal(controller.delivery().document) &&
           editing.settings().writes() == 0U && editing.files().written().empty() &&
           editing.bookmarks().read().value().files.empty() &&
           editing.history().read().value().files.empty() && controller.vim_state().incsearch &&
           controller.vim_state().highlight == core::VimSearchHighlight::on;
}
} // namespace

std::expected<std::uint64_t, ProbeFailure> probed_controller_input(CommandInputWorkload workload,
                                                                   const std::string &input,
                                                                   application::TimingPort &timing)
{
    const auto &sample = controller_spec(workload);
    Editing editing;
    prepare_controller(editing, sample, input);
    const auto initial = editing.controller().frame();
    const auto delivered = editing.controller().delivery();
    if (!initial.command_line.has_value() || !controller_equal(editing, sample, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::string_view held_input = initial.command_line.value().text;
    const std::string_view held_body = initial.lines.at(0U).text;
    if (sample.stages_fill && !fill_is_staged(editing, sample, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto requests_before = editing.folders().requests().size();
    const auto checksum = cycled_controller(editing.controller(), sample, timing);
    if (checksum != sample.checksum || !controller_equal(editing, sample, input) ||
        !frame_equal(initial, sample, input) || held_input != input || held_body != "a x a" ||
        !document_equal(delivered.document) ||
        !core::same_settings(delivered.settings, core::default_editor_settings()) ||
        editing.folders().requests().size() != requests_before + sample.new_requests)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
