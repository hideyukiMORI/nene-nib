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
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] core::SelectionSpan match_span(std::size_t begin, std::size_t end)
{
    return {core::SelectionPresence::present, core::Column{begin}, core::Column{end}};
}

[[nodiscard]] bool body_equal(const application::EditorFrame &frame, CommandInputWorkload workload)
{
    if (frame.lines.size() != 1U)
    {
        return false;
    }
    const auto &line = frame.lines.at(0U);
    const std::vector<core::SelectionSpan> matches =
        workload == CommandInputWorkload::controller_search
            ? std::vector{match_span(1U, 2U), match_span(5U, 6U)}
            : std::vector<core::SelectionSpan>{};
    const std::optional<core::SelectionSpan> current =
        workload == CommandInputWorkload::controller_search ? std::optional{match_span(5U, 6U)}
                                                            : std::nullopt;
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
                                 CommandInputWorkload workload)
{
    if (workload == CommandInputWorkload::controller_search ||
        workload == CommandInputWorkload::controller_ex)
    {
        return !frame.command_palette.has_value() && frame.command_line.value().completions.empty();
    }
    if (!frame.command_palette.has_value())
    {
        return false;
    }
    const auto &palette = frame.command_palette.value();
    if (workload == CommandInputWorkload::controller_palette)
    {
        return palette.rows.empty() && palette.total == 0U && palette.selected == 0U &&
               palette.first == 0U && frame.command_line.value().completions.empty();
    }
    if (palette.rows.size() != 1U || palette.total != 1U || palette.selected != 0U ||
        palette.first != 0U)
    {
        return false;
    }
    const auto &choice = palette.rows.at(0U);
    return choice.kind == core::CommandChoiceKind::fill && choice.command == "set fontsize=" &&
           choice.label.text() == "set fontsize=" &&
           frame.command_line.value().completions == std::vector<std::string>{"set fontsize="};
}

[[nodiscard]] bool frame_equal(const application::EditorFrame &frame, CommandInputWorkload workload,
                               std::string_view input)
{
    if (!frame.command_line.has_value())
    {
        return false;
    }
    const auto &line = frame.command_line.value();
    const auto prompt =
        workload == CommandInputWorkload::controller_search ? core::InputLinePrompt::search_forward
        : workload == CommandInputWorkload::controller_ex   ? core::InputLinePrompt::ex
                                                            : core::InputLinePrompt::palette;
    return body_equal(frame, workload) && document_equal(frame.document) &&
           frame.tabs.size() == 1U && document_equal(frame.tabs.at(0U)) && frame.active_tab == 0U &&
           line.text == input && line.caret.value == input.size() &&
           core::code_point_count(line.text) == input.size() && line.prompt == prompt &&
           !line.completion_index.has_value() && palette_equal(frame, workload) &&
           !frame.composition.has_value() && !frame.command_composition.has_value() &&
           !frame.command_message.has_value() && !frame.settings_failure.has_value() &&
           core::same_settings(frame.settings, core::default_editor_settings()) &&
           frame.status_items.at(0U).text() == "行 1, 桁 1" &&
           frame.status_items.at(1U).text() == "UTF-8" &&
           frame.status_items.at(2U).text() == "CRLF";
}

void prepare_controller(Editing &editing, CommandInputWorkload workload, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold("a x a");
    static_cast<void>(controller.apply(application::VisibleLines{1U}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\probe.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    if (workload == CommandInputWorkload::controller_search ||
        workload == CommandInputWorkload::controller_ex)
    {
        const char32_t key = workload == CommandInputWorkload::controller_search ? U'/' : U':';
        static_cast<void>(controller.apply(application::VimKeyPress{core::VimCharacter{key}}));
    }
    if (workload == CommandInputWorkload::controller_palette ||
        workload == CommandInputWorkload::controller_fill)
    {
        static_cast<void>(controller.apply(application::OpenCommandPalette{}));
    }
    static_cast<void>(controller.apply(application::CommandText{input}));
}

[[nodiscard]] bool fill_is_staged(Editing &editing, const std::string &input)
{
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(application::SubmitCommand{}));
    const auto filled = controller.frame();
    if (!frame_equal(filled, CommandInputWorkload::controller_fill, ":set fontsize=") ||
        editing.settings().writes() != 0U)
    {
        return false;
    }
    static_cast<void>(controller.apply(application::CancelCommand{}));
    static_cast<void>(controller.apply(application::OpenCommandPalette{}));
    static_cast<void>(controller.apply(application::CommandText{input}));
    return frame_equal(controller.frame(), CommandInputWorkload::controller_fill, input);
}

[[nodiscard]] std::uint64_t cycled_controller(application::EditorController &controller,
                                              CommandInputWorkload workload,
                                              application::TimingPort &timing)
{
    const std::vector<application::EditorIntent> intents =
        workload == CommandInputWorkload::controller_fill
            ? std::vector<application::EditorIntent>{application::SubmitCommand{},
                                                     application::CancelCommand{},
                                                     application::OpenCommandPalette{},
                                                     application::CommandText{":fz"}}
            : std::vector<application::EditorIntent>{
                  application::CommandText{"b"},
                  application::EditCommand{core::CommandEdit::backspace}};
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t cycle = 0; cycle < 64U; ++cycle)
    {
        for (const auto &intent : intents)
        {
            const auto delivery = controller.apply(intent);
            checksum += delivery.document.title.text().size();
        }
    }
    timing.mark(core::Milestone::probe_finished);
    return checksum;
}

[[nodiscard]] bool controller_equal(Editing &editing, CommandInputWorkload workload,
                                    const std::string &input)
{
    const auto &controller = editing.controller();
    const auto documents = controller.documents();
    return frame_equal(controller.frame(), workload, input) && documents.size() == 1U &&
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
    Editing editing;
    prepare_controller(editing, workload, input);
    const auto initial = editing.controller().frame();
    const auto delivered = editing.controller().delivery();
    if (!controller_equal(editing, workload, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::string_view held_input = initial.command_line.value().text;
    const std::string_view held_body = initial.lines.at(0U).text;
    if (workload == CommandInputWorkload::controller_fill && !fill_is_staged(editing, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto requests_before = editing.folders().requests().size();
    const auto checksum = cycled_controller(editing.controller(), workload, timing);
    const std::uint64_t expected =
        workload == CommandInputWorkload::controller_fill ? 2304U : 1152U;
    const std::size_t new_requests = workload == CommandInputWorkload::controller_fill ? 64U : 0U;
    if (checksum != expected || !controller_equal(editing, workload, input) ||
        !frame_equal(initial, workload, input) || held_input != input || held_body != "a x a" ||
        !document_equal(delivered.document) ||
        !core::same_settings(delivered.settings, core::default_editor_settings()) ||
        editing.folders().requests().size() != requests_before + new_requests)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
