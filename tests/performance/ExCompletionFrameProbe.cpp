#include "ExCompletionFrameProbe.hpp"
#include "CommandText.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "Milestone.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"
#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace nenenib::tests::performance
{
namespace
{
constexpr std::string_view probe_title = "probe.txt";
[[nodiscard]] application::EditorIntent opening_of(ExCompletionWorkload workload)
{
    switch (workload)
    {
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
        return application::VimKeyPress{core::VimCharacter{U':'}};
    case ExCompletionWorkload::frame_palette:
        return application::OpenCommandPalette{};
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::custom_wide:
    case ExCompletionWorkload::all_custom:
        break;
    }
    std::unreachable();
}
[[nodiscard]] core::InputLinePrompt prompt_of(ExCompletionWorkload workload)
{
    switch (workload)
    {
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
        return core::InputLinePrompt::ex;
    case ExCompletionWorkload::frame_palette:
        return core::InputLinePrompt::palette;
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::custom_wide:
    case ExCompletionWorkload::all_custom:
        break;
    }
    std::unreachable();
}
[[nodiscard]] bool body_equal(const application::EditorFrame &frame)
{
    if (frame.lines.size() != 1U)
    {
        return false;
    }
    const auto &line = frame.lines.at(0U);
    return line.number.value == 1U && line.text == "a x a" && line.display.text == "a x a" &&
           line.display.starts == std::vector<std::size_t>{0U, 1U, 2U, 3U, 4U, 5U} &&
           line.selection == core::no_selection_span() && line.matches.empty() &&
           !line.current_match.has_value() && frame.caret.position.line.value == 1U &&
           frame.caret.position.column.value == 1U && frame.first_visible.value == 1U &&
           frame.total_lines == 1U && frame.mode == core::EditMode::vim &&
           frame.vim_mode == core::VimMode::normal;
}
[[nodiscard]] bool document_equal(const application::DocumentView &document)
{
    return document.title.text() == probe_title && document.title.code_point_count() == 9U &&
           document.path.has_value() &&
           document.path.value().text() == "C:\\nib-probe\\probe.txt" &&
           document.save_state == core::SaveState::saved &&
           document.encoding == core::TextEncoding::utf8 && !document.last_failure.has_value();
}
[[nodiscard]] bool palette_equal(const application::EditorFrame &frame,
                                 core::InputLinePrompt prompt)
{
    switch (prompt)
    {
    case core::InputLinePrompt::ex:
        return !frame.command_palette.has_value();
    case core::InputLinePrompt::palette:
        break;
    case core::InputLinePrompt::search_forward:
    case core::InputLinePrompt::search_backward:
    case core::InputLinePrompt::find:
    case core::InputLinePrompt::replace_with:
    case core::InputLinePrompt::go_to_line:
        return false;
    }
    if (!frame.command_palette.has_value())
    {
        return false;
    }
    const auto &palette = frame.command_palette.value();
    if (palette.rows.size() != 1U || palette.total != 1U || palette.selected != 0U ||
        palette.first != 0U)
    {
        return false;
    }
    const auto &choice = palette.rows.at(0U);
    return choice.kind == core::CommandChoiceKind::fill && choice.command == "set fontsize=" &&
           choice.label.text() == "set fontsize=";
}
[[nodiscard]] bool frame_equal(const application::EditorFrame &frame,
                               const ExCompletionSample &sample,
                               std::span<const std::string> expected, core::InputLinePrompt prompt)
{
    if (!frame.command_line.has_value())
    {
        return false;
    }
    const auto &line = frame.command_line.value();
    return body_equal(frame) && document_equal(frame.document) && frame.tabs.size() == 1U &&
           document_equal(frame.tabs.at(0U)) && frame.active_tab == 0U &&
           line.text == sample.prefix && line.caret.value == sample.prefix.size() &&
           line.prompt == prompt && !line.completion_index.has_value() &&
           completions_equal(line.completions, expected) && palette_equal(frame, prompt) &&
           !frame.composition.has_value() && !frame.command_composition.has_value() &&
           !frame.command_message.has_value() && !frame.settings_failure.has_value() &&
           core::same_settings(frame.settings, core::default_editor_settings()) &&
           frame.status_items.at(0U).text() == "行 1, 桁 1" &&
           frame.status_items.at(1U).text() == "UTF-8" &&
           frame.status_items.at(2U).text() == "CRLF";
}
void prepare(Editing &editing, ExCompletionWorkload workload, const ExCompletionSample &sample)
{
    auto &controller = editing.controller();
    editing.files().hold("a x a");
    static_cast<void>(controller.apply(application::VisibleLines{1U}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\probe.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(controller.apply(opening_of(workload)));
    if (!sample.prefix.empty())
    {
        static_cast<void>(controller.apply(application::CommandText{std::string(sample.prefix)}));
    }
}
[[nodiscard]] bool controller_equal(Editing &editing, const ExCompletionSample &sample,
                                    std::span<const std::string> expected,
                                    core::InputLinePrompt prompt)
{
    auto &controller = editing.controller();
    const auto documents = controller.documents();
    return frame_equal(controller.frame(), sample, expected, prompt) && documents.size() == 1U &&
           document_equal(documents.at(0U)) && document_equal(controller.delivery().document) &&
           editing.settings().writes() == 0U && editing.files().written().empty() &&
           editing.bookmarks().read().value().files.empty() &&
           editing.history().read().value().files.empty() && controller.vim_state().incsearch &&
           controller.vim_state().highlight == core::VimSearchHighlight::on;
}
[[nodiscard]] std::uint64_t generated_frames(application::EditorController &controller,
                                             application::TimingPort &timing)
{
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < 64U; ++index)
    {
        const auto frame = controller.frame();
        if (!frame.command_line.has_value())
        {
            return 0;
        }
        const auto &line = frame.command_line.value();
        checksum += completion_checksum(line.completions) + frame.document.title.text().size() +
                    line.text.size();
    }
    timing.mark(core::Milestone::probe_finished);
    return checksum;
}
} // namespace
std::expected<std::uint64_t, ProbeFailure>
probed_ex_completion_frame(ExCompletionWorkload workload, const ExCompletionSample &sample,
                           application::TimingPort &timing)
{
    Editing editing;
    const auto prompt = prompt_of(workload);
    const auto expected = expected_completions(workload);
    prepare(editing, workload, sample);
    const auto held = editing.controller().frame();
    if (!held.command_line.has_value() || !controller_equal(editing, sample, expected, prompt))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &held_line = held.command_line.value();
    const std::string_view held_input = held_line.text;
    const std::string_view held_body = held.lines.at(0U).text;
    std::vector<std::string_view> views;
    for (const auto &candidate : held_line.completions)
    {
        views.emplace_back(candidate);
    }
    const auto requests = editing.folders().requests().size();
    const auto checksum = generated_frames(editing.controller(), timing);
    // 64 枚の frame それぞれが候補・題名・入力の長さを足す（generated_frames）。
    const std::uint64_t per_frame =
        completion_checksum(expected) + probe_title.size() + sample.prefix.size();
    if (checksum != 64U * per_frame || !controller_equal(editing, sample, expected, prompt) ||
        !frame_equal(held, sample, expected, prompt) || held_input != sample.prefix ||
        held_body != "a x a" || !std::ranges::equal(views, expected) ||
        editing.folders().requests().size() != requests)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
