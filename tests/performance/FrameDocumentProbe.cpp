#include "FrameDocumentProbe.hpp"

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
#include "VisibleLines.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] bool has_long_path(FrameDocumentWorkload workload)
{
    return workload == FrameDocumentWorkload::long_saved ||
           workload == FrameDocumentWorkload::long_failed;
}

[[nodiscard]] std::string document_title(FrameDocumentWorkload workload)
{
    switch (workload)
    {
    case FrameDocumentWorkload::short_saved:
        return "frame-document.txt";
    case FrameDocumentWorkload::long_saved:
        return std::string(64U, 'a') + ".txt";
    case FrameDocumentWorkload::long_failed:
        return "● " + std::string(64U, 'a') + ".txt";
    case FrameDocumentWorkload::untitled:
        return "無題";
    }
    std::unreachable();
}

void prepare_document(Editing &editing, FrameDocumentWorkload workload, const std::string &input)
{
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(application::VisibleLines{2U}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
    if (workload == FrameDocumentWorkload::untitled)
    {
        return;
    }
    const std::string body = has_long_path(workload) ? "\xEF\xBB\xBF"
                                                       "a日\t🖋\x01\r\nend"
                                                     : "a日\t🖋\x01\nend";
    editing.files().hold(body);
    const auto path = core::FilePath::parse(input).value();
    static_cast<void>(controller.apply(application::OpenDocument{path}));
    if (workload == FrameDocumentWorkload::long_failed)
    {
        static_cast<void>(controller.apply(application::InsertText{"x"}));
        static_cast<void>(controller.apply(application::PlaceCaret{
            {core::LineNumber{1U}, core::Column{1U}}, core::SelectionAnchoring::collapse}));
        editing.files().refuse_writes(application::FileFailure::access_denied);
        static_cast<void>(
            controller.apply(application::SaveDocument{path, core::TextEncoding::utf8_bom}));
    }
}

[[nodiscard]] bool document_equal(const application::DocumentView &document,
                                  FrameDocumentWorkload workload, const std::string &input)
{
    const bool named = workload != FrameDocumentWorkload::untitled;
    const auto encoding =
        has_long_path(workload) ? core::TextEncoding::utf8_bom : core::TextEncoding::utf8;
    const auto saved = workload == FrameDocumentWorkload::long_failed ? core::SaveState::modified
                                                                      : core::SaveState::saved;
    const std::optional<application::FileFailure> failure =
        workload == FrameDocumentWorkload::long_failed
            ? std::optional{application::FileFailure::access_denied}
            : std::nullopt;
    const bool path_equal =
        document.path.has_value() ? document.path.value().text() == input : input.empty();
    return document.title.text() == document_title(workload) &&
           document.path.has_value() == named && path_equal && document.encoding == encoding &&
           document.save_state == saved && document.last_failure == failure;
}

[[nodiscard]] bool line_equal(const application::LineView &line, std::size_t number,
                              std::string_view body, const core::DisplayLine &display)
{
    return line.number.value == number && line.text == body && line.display.text == display.text &&
           line.display.starts == display.starts && line.selection == core::no_selection_span() &&
           line.matches.empty() && !line.current_match.has_value();
}

[[nodiscard]] bool body_equal(const application::EditorFrame &frame, FrameDocumentWorkload workload)
{
    if (workload == FrameDocumentWorkload::untitled)
    {
        return frame.lines.size() == 1U &&
               line_equal(frame.lines.at(0U), 1U, "", core::DisplayLine{"", {0U}});
    }
    if (frame.lines.size() != 2U ||
        !line_equal(frame.lines.at(1U), 2U, "end", core::DisplayLine{"end", {0U, 1U, 2U, 3U}}))
    {
        return false;
    }
    if (workload == FrameDocumentWorkload::long_failed)
    {
        return line_equal(frame.lines.at(0U), 1U, "xa日\t🖋\x01",
                          core::DisplayLine{"xa日\t🖋^A", {0U, 1U, 2U, 3U, 4U, 5U, 7U}});
    }
    return line_equal(frame.lines.at(0U), 1U, "a日\t🖋\x01",
                      core::DisplayLine{"a日\t🖋^A", {0U, 1U, 2U, 3U, 4U, 6U}});
}

[[nodiscard]] bool frame_equal(const application::EditorFrame &frame,
                               FrameDocumentWorkload workload, const std::string &input)
{
    const std::size_t rows = workload == FrameDocumentWorkload::untitled ? 1U : 2U;
    const std::string_view encoding = has_long_path(workload) ? "UTF-8 BOM" : "UTF-8";
    const std::string_view ending = workload == FrameDocumentWorkload::short_saved ? "LF" : "CRLF";
    const application::CaretView caret{{core::LineNumber{1U}, core::Column{1U}},
                                       core::CaretShape::bar};
    return body_equal(frame, workload) && document_equal(frame.document, workload, input) &&
           frame.tabs.size() == 1U && document_equal(frame.tabs.at(0U), workload, input) &&
           frame.active_tab == 0U && frame.first_visible.value == 1U && frame.total_lines == rows &&
           frame.caret == caret && frame.mode == core::EditMode::ordinary &&
           frame.vim_mode == core::VimMode::normal && !frame.composition.has_value() &&
           !frame.command_composition.has_value() && !frame.command_line.has_value() &&
           !frame.command_palette.has_value() && !frame.command_message.has_value() &&
           frame.status_items.at(0U).text() == "行 1, 桁 1" &&
           frame.status_items.at(1U).text() == encoding &&
           frame.status_items.at(2U).text() == ending;
}

[[nodiscard]] bool controller_equal(const application::EditorController &controller,
                                    FrameDocumentWorkload workload, const std::string &input)
{
    const auto documents = controller.documents();
    return documents.size() == 1U && document_equal(documents.at(0U), workload, input) &&
           document_equal(controller.delivery().document, workload, input) &&
           frame_equal(controller.frame(), workload, input);
}

[[nodiscard]] std::uint64_t document_checksum(const application::EditorFrame &frame)
{
    const auto &document = frame.document;
    const std::size_t path_bytes =
        document.path.has_value() ? document.path.value().text().size() : 0U;
    return document.title.text().size() + path_bytes + frame.status_items.at(1U).text().size() +
           frame.tabs.size() + frame.lines.size();
}
} // namespace

std::string frame_document_input(FrameDocumentWorkload workload)
{
    if (workload == FrameDocumentWorkload::untitled)
    {
        return "";
    }
    if (workload == FrameDocumentWorkload::short_saved)
    {
        return "C:\\nib-probe\\frame-document.txt";
    }
    std::string path = "C:\\nib-probe\\";
    for (std::size_t index = 0; index < 20U; ++index)
    {
        path += "segment\\";
    }
    return path + std::string(64U, 'a') + ".txt";
}

std::expected<std::uint64_t, ProbeFailure> framed_documents(FrameDocumentWorkload workload,
                                                            const std::string &input,
                                                            application::TimingPort &timing)
{
    if (input != frame_document_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    Editing editing;
    prepare_document(editing, workload, input);
    const auto &controller = editing.controller();
    const auto initial = controller.frame();
    const auto delivered = controller.delivery();
    if (!frame_equal(initial, workload, input) || !controller_equal(controller, workload, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t iteration = 0; iteration < 256U; ++iteration)
    {
        const auto frame = controller.frame();
        checksum += document_checksum(frame);
    }
    timing.mark(core::Milestone::probe_finished);
    constexpr std::array<std::uint64_t, 4> checksums{14592U, 82176U, 83200U, 3328U};
    if (checksum != checksums.at(static_cast<std::size_t>(workload)) ||
        !controller_equal(controller, workload, input) || !frame_equal(initial, workload, input) ||
        !document_equal(delivered.document, workload, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
