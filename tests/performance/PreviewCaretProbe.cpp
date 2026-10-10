#include "PreviewCaretProbe.hpp"

#include "CommandText.hpp"
#include "Editing.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
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

#include <cstdio>
#include <optional>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::size_t prefix_count(PreviewCaretWorkload workload)
{
    return workload == PreviewCaretWorkload::short_thirty ? 1U : 1024U;
}

[[nodiscard]] std::size_t row_count(PreviewCaretWorkload workload)
{
    return workload == PreviewCaretWorkload::long_full ? 120U : 30U;
}

[[nodiscard]] core::FilePath preview_path()
{
    return core::FilePath::parse("C:\\nib-probe\\preview-caret.txt").value();
}

[[nodiscard]] std::string preview_head(PreviewCaretWorkload workload)
{
    std::string head;
    for (std::size_t at = 0; at < prefix_count(workload); ++at)
    {
        head += "日";
    }
    return head + "z\t🖋\x01";
}

void preview_key(application::EditorController &controller, char32_t key)
{
    static_cast<void>(controller.apply(application::VimKeyPress{core::VimCharacter{key}}));
}

void prepare_preview(Editing &editing, PreviewCaretWorkload workload, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(application::VisibleLines{row_count(workload)}));
    static_cast<void>(controller.apply(application::OpenDocument{preview_path()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    if (workload == PreviewCaretWorkload::unsearched)
    {
        return;
    }
    if (workload == PreviewCaretWorkload::confirmed)
    {
        static_cast<void>(controller.apply(application::VimKeyPress{
            core::VimSearchPattern{"z", core::VimSearchDirection::forward, std::nullopt}}));
        static_cast<void>(controller.apply(application::PlaceCaret{
            {core::LineNumber{1U}, core::Column{1U}}, core::SelectionAnchoring::collapse}));
        return;
    }
    if (workload == PreviewCaretWorkload::disabled)
    {
        preview_key(controller, U':');
        static_cast<void>(controller.apply(application::CommandText{"set noincsearch"}));
        static_cast<void>(controller.apply(application::SubmitCommand{}));
        preview_key(controller, U'h');
    }
    preview_key(controller, U'/');
    const std::string query = workload == PreviewCaretWorkload::absent ? "q" : "z";
    static_cast<void>(controller.apply(application::CommandText{query}));
}

[[nodiscard]] bool preview_is_active(PreviewCaretWorkload workload)
{
    return workload == PreviewCaretWorkload::long_thirty ||
           workload == PreviewCaretWorkload::long_full ||
           workload == PreviewCaretWorkload::short_thirty;
}

[[nodiscard]] bool preview_map_equal(const core::DisplayLine &display, std::size_t prefix)
{
    if (display.starts.size() != prefix + 5U)
    {
        return false;
    }
    for (std::size_t at = 0; at < display.starts.size(); ++at)
    {
        const std::size_t expected = at == prefix + 4U ? at + 1U : at;
        if (display.starts.at(at) != expected)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool preview_text_equal(const application::LineView &line,
                                      PreviewCaretWorkload workload)
{
    if (line.number.value != 1U)
    {
        return line.text == "z x" && line.display.text == "z x" &&
               line.display.starts == std::vector<std::size_t>{0U, 1U, 2U, 3U};
    }
    const std::string body = preview_head(workload);
    return line.text == body && line.display.text == body.substr(0U, body.size() - 1U) + "^A" &&
           preview_map_equal(line.display, prefix_count(workload));
}

[[nodiscard]] bool preview_line_equal(const application::LineView &line,
                                      PreviewCaretWorkload workload)
{
    const std::size_t column = line.number.value == 1U ? prefix_count(workload) + 1U : 1U;
    const core::SelectionSpan span{core::SelectionPresence::present, core::Column{column},
                                   core::Column{column + 1U}};
    std::vector<core::SelectionSpan> matches;
    if (preview_is_active(workload) || workload == PreviewCaretWorkload::confirmed)
    {
        matches.push_back(span);
    }
    const auto current =
        preview_is_active(workload) && line.number.value == 1U ? std::optional{span} : std::nullopt;
    return preview_text_equal(line, workload) && line.selection == core::no_selection_span() &&
           line.matches == matches && line.current_match == current;
}

[[nodiscard]] bool preview_input_equal(const application::EditorFrame &frame,
                                       PreviewCaretWorkload workload)
{
    if (workload == PreviewCaretWorkload::confirmed || workload == PreviewCaretWorkload::unsearched)
    {
        return !frame.command_line.has_value();
    }
    if (!frame.command_line.has_value())
    {
        return false;
    }
    const auto &line = frame.command_line.value();
    return line.prompt == core::InputLinePrompt::search_forward &&
           line.text == (workload == PreviewCaretWorkload::absent ? "q" : "z") &&
           line.caret.value == 1U && line.completions.empty() && !line.completion_index.has_value();
}

[[nodiscard]] bool preview_layout_equal(const application::EditorFrame &frame,
                                        PreviewCaretWorkload workload)
{
    return frame.lines.size() == row_count(workload) && frame.total_lines == 120U &&
           frame.first_visible.value == 1U && frame.caret.position.line.value == 1U &&
           frame.caret.position.column.value == 1U && frame.mode == core::EditMode::vim &&
           frame.vim_mode == core::VimMode::normal && !frame.command_message.has_value() &&
           !frame.composition.has_value() && !frame.command_composition.has_value() &&
           !frame.document.last_failure.has_value() && preview_input_equal(frame, workload);
}

[[nodiscard]] bool preview_state_equal(const application::EditorController &controller,
                                       PreviewCaretWorkload workload)
{
    const auto vim = controller.vim_state();
    const auto frame = controller.frame();
    if (vim.last_search.has_value() != (workload == PreviewCaretWorkload::confirmed) ||
        vim.incsearch != (workload != PreviewCaretWorkload::disabled) ||
        vim.highlight != core::VimSearchHighlight::on || !preview_layout_equal(frame, workload))
    {
        return false;
    }
    for (std::size_t at = 0; at < frame.lines.size(); ++at)
    {
        if (frame.lines.at(at).number.value != at + 1U ||
            !preview_line_equal(frame.lines.at(at), workload))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool preview_saved_equal(Editing &editing, const std::string &input)
{
    const auto delivered = editing.controller().apply(
        application::SaveDocument{preview_path(), core::TextEncoding::utf8});
    return !delivered.document.last_failure.has_value() &&
           delivered.document.save_state == core::SaveState::saved &&
           editing.files().written_path() == preview_path().text() &&
           editing.files().written() == input;
}

[[nodiscard]] std::uint64_t preview_frames(const application::EditorController &controller,
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

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure> preview_failed(const char *stage)
{
    std::fprintf(stderr, "preview caret probe failed: %s\n", stage);
    return std::unexpected(ProbeFailure::wrong_result);
}
} // namespace

std::string preview_caret_input(PreviewCaretWorkload workload)
{
    std::string body = preview_head(workload);
    for (std::size_t at = 1U; at < 120U; ++at)
    {
        body += "\r\nz x";
    }
    return body;
}

std::expected<std::uint64_t, ProbeFailure> probed_preview_caret(PreviewCaretWorkload workload,
                                                                const std::string &input,
                                                                application::TimingPort &timing)
{
    if (input != preview_caret_input(workload))
    {
        return preview_failed("fixed input");
    }
    Editing editing;
    prepare_preview(editing, workload, input);
    if (!preview_state_equal(editing.controller(), workload) ||
        !preview_saved_equal(editing, input))
    {
        return preview_failed("before values and whole body");
    }
    const std::uint64_t expected = 64U * (row_count(workload) + 123U);
    if (preview_frames(editing.controller(), timing) != expected)
    {
        return preview_failed("timed checksum");
    }
    if (!preview_state_equal(editing.controller(), workload) ||
        !preview_saved_equal(editing, input))
    {
        return preview_failed("after values and whole body");
    }
    return checksum_of(editing.files().written());
}
} // namespace nenenib::tests::performance
