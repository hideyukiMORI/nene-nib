#include "ProbeWorkload.hpp"

#include "BufferProbe.hpp"
#include "CommandInputProbe.hpp"
#include "CompletionPrefixProbe.hpp"
#include "ExCompletionProbe.hpp"
#include "FileNameProbe.hpp"
#include "FrameDocumentProbe.hpp"
#include "FrameProbe.hpp"
#include "FrameRowsProbe.hpp"
#include "LexerProbe.hpp"
#include "MarkdownProbe.hpp"
#include "NewlineIndexProbe.hpp"
#include "OperationChoiceProbe.hpp"
#include "PaletteChoiceProbe.hpp"
#include "PatternProbe.hpp"
#include "PreviewCaretProbe.hpp"
#include "ProbeDispatch.hpp"
#include "ProbeSelection.hpp"
#include "ScopedProbe.hpp"
#include "SearchProbe.hpp"
#include "SearchSnapshotProbe.hpp"
#include "SearchSpanProbe.hpp"
#include "StatusItemsProbe.hpp"
#include "TabDisplayProbe.hpp"
#include "TextObjectProbe.hpp"

#include "DeleteText.hpp"
#include "DisplayLine.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "StoreVimRegister.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSpecialKey.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t large_lines = 200000;
constexpr std::size_t large_line_bytes = 84;
constexpr std::size_t insert_count = 200;

[[nodiscard]] core::FilePath probe_path()
{
    return core::FilePath::parse("C:\\nib-probe\\fixed.txt").value();
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure> opened(const std::string &input,
                                                                application::TimingPort &timing)
{
    Editing editing;
    editing.files().hold(input);
    static_cast<void>(editing.controller().apply(application::VisibleLines{30}));
    const application::OpenDocument intent{probe_path()};
    timing.mark(core::Milestone::probe_started);
    const auto delivered = editing.controller().apply(intent);
    timing.mark(core::Milestone::probe_finished);
    const auto frame = editing.controller().frame();
    static_cast<void>(editing.controller().apply(
        application::SaveDocument{probe_path(), core::TextEncoding::utf8}));
    if (delivered.document.last_failure.has_value() ||
        delivered.document.encoding != core::TextEncoding::utf8 ||
        frame.total_lines != large_lines + 1 || frame.caret.position.column.value != 1 ||
        frame.caret.position.line.value != 1 || editing.files().reads() != 1 ||
        editing.files().written() != input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(editing.files().written());
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
saved(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto encoding = workload == ProbeWorkload::controller_save ? core::TextEncoding::utf8
                                                                     : core::TextEncoding::utf8_bom;
    Editing editing;
    editing.files().hold(input);
    static_cast<void>(editing.controller().apply(application::VisibleLines{30}));
    const auto opened = editing.controller().apply(application::OpenDocument{probe_path()});
    if (opened.document.last_failure.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::string expected =
        (encoding == core::TextEncoding::utf8_bom ? "\xEF\xBB\xBF" : "") + input;
    const application::SaveDocument intent{probe_path(), encoding};
    timing.mark(core::Milestone::probe_started);
    const auto delivered = editing.controller().apply(intent);
    timing.mark(core::Milestone::probe_finished);
    const auto frame = editing.controller().frame();
    if (delivered.document.last_failure.has_value() || delivered.document.encoding != encoding ||
        delivered.document.save_state != core::SaveState::saved ||
        !delivered.document.path.has_value() || delivered.document.path.value() != probe_path() ||
        delivered.document.title.text() != opened.document.title.text() ||
        frame.total_lines != large_lines + 1U || frame.status_items.at(2U).text() != "CRLF" ||
        editing.files().written_path() != probe_path().text() ||
        editing.files().written() != expected)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto checksum = checksum_of(editing.files().written());
    static_cast<void>(editing.controller().apply(
        application::SaveDocument{probe_path(), core::TextEncoding::utf8}));
    if (editing.files().written() != input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure> buffered(const std::string &input,
                                                                  application::TimingPort &timing)
{
    timing.mark(core::Milestone::probe_started);
    const auto result = core::TextBuffer::from_utf8(input);
    timing.mark(core::Milestone::probe_finished);
    if (!result.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &text = result.value();
    const std::string observed = text.text();
    if (observed != input || text.line_count() != large_lines + 1 ||
        text.line_start(core::LineNumber{2}).value != large_line_bytes ||
        text.line_start(core::LineNumber{large_lines + 1}).value != input.size())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(observed);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
inserted_into(Editing &editing, std::string_view input, application::TimingPort &timing)
{
    const application::InsertText intent{"a"};
    std::optional<decltype(editing.controller().apply(intent))> delivered;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < insert_count; ++index)
    {
        delivered = editing.controller().apply(intent);
    }
    timing.mark(core::Milestone::probe_finished);
    const auto frame = editing.controller().frame();
    static_cast<void>(editing.controller().apply(
        application::SaveDocument{probe_path(), core::TextEncoding::utf8}));
    if (!delivered.has_value() || delivered.value().document.last_failure.has_value() ||
        frame.total_lines != 1 || frame.caret.position.column.value != insert_count + 1 ||
        frame.caret.position.line.value != 1 || editing.files().written() != input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(editing.files().written());
}

[[nodiscard]] bool saved_equals(Editing &editing, std::string_view expected)
{
    const auto delivered = editing.controller().apply(
        application::SaveDocument{probe_path(), core::TextEncoding::utf8});
    return !delivered.document.last_failure.has_value() && editing.files().written() == expected;
}

[[nodiscard]] bool history_restored(Editing &editing, const std::string &original)
{
    const application::HistoryAction undo{core::HistoryDirection::undo};
    const application::HistoryAction redo{core::HistoryDirection::redo};
    static_cast<void>(editing.controller().apply(undo));
    if (!saved_equals(editing, ""))
    {
        return false;
    }
    static_cast<void>(editing.controller().apply(undo));
    if (!saved_equals(editing, original))
    {
        return false;
    }
    static_cast<void>(editing.controller().apply(redo));
    if (!saved_equals(editing, ""))
    {
        return false;
    }
    static_cast<void>(editing.controller().apply(redo));
    return saved_equals(editing, std::string(insert_count, 'a'));
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure> inserted(const std::string &input,
                                                                  application::TimingPort &timing)
{
    Editing editing;
    static_cast<void>(editing.controller().apply(application::VisibleLines{30}));
    return inserted_into(editing, input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
inserted_after_delete(const std::string &input, application::TimingPort &timing)
{
    Editing editing;
    editing.files().hold(input);
    static_cast<void>(editing.controller().apply(application::VisibleLines{30}));
    const auto opened = editing.controller().apply(application::OpenDocument{probe_path()});
    if (opened.document.last_failure.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    static_cast<void>(editing.controller().apply(application::SelectAll{}));
    static_cast<void>(
        editing.controller().apply(application::DeleteText{core::DeleteDirection::backward}));
    const auto frame = editing.controller().frame();
    if (frame.total_lines != 1 || frame.caret.position.column.value != 1 ||
        !saved_equals(editing, ""))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto result = inserted_into(editing, std::string(insert_count, 'a'), timing);
    if (!result.has_value() || !history_restored(editing, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return result.value() ^ checksum_of(input);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure> displayed(const std::string &input,
                                                                   application::TimingPort &timing)
{
    const std::size_t characters = core::code_point_count(input);
    timing.mark(core::Milestone::probe_started);
    const auto line = core::display_line(input);
    timing.mark(core::Milestone::probe_finished);
    if (line.text != input || line.starts.size() != characters + 1)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    for (std::size_t index = 0; index < line.starts.size(); ++index)
    {
        if (line.starts[index] != index)
        {
            return std::unexpected(ProbeFailure::wrong_result);
        }
    }
    return checksum_of(line.text) ^ static_cast<std::uint64_t>(line.starts.size());
}
} // namespace

namespace
{
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
validated(const std::string &input, std::size_t expected, application::TimingPort &timing)
{
    timing.mark(core::Milestone::probe_started);
    const auto result = core::validate_utf8(input);
    timing.mark(core::Milestone::probe_finished);
    if (!result.has_value() || result.value() != expected)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) ^ static_cast<std::uint64_t>(result.value());
}
} // namespace

namespace
{
// 準備・照合の文字キーも製品の入口へ流す。Vim harnessの別経路は作らない。
void vim_characters(Editing &editing, std::u32string_view keys)
{
    for (const char32_t key : keys)
    {
        static_cast<void>(
            editing.controller().apply(application::VimKeyPress{core::VimCharacter{key}}));
    }
}

void vim_escape(Editing &editing)
{
    static_cast<void>(
        editing.controller().apply(application::VimKeyPress{core::VimSpecialKey::escape}));
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_inserted(Editing &editing, std::size_t count, application::TimingPort &timing)
{
    const application::VimKeyPress intent{core::VimCharacter{U'x'}};
    std::optional<decltype(editing.controller().apply(intent))> delivered;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t index = 0; index < count; ++index)
    {
        delivered = editing.controller().apply(intent);
    }
    timing.mark(core::Milestone::probe_finished);
    const auto frame = editing.controller().frame();
    const std::string expected(count, 'x');
    if (!delivered.has_value() || delivered.value().document.last_failure.has_value() ||
        frame.mode != core::EditMode::vim || frame.vim_mode != core::VimMode::insert ||
        frame.total_lines != 1 || frame.caret.position.line.value != 1 ||
        frame.caret.position.column.value != count + 1 || !saved_equals(editing, expected))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(editing.files().written());
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_inserted_with_register(const std::string &input, application::TimingPort &timing)
{
    Editing editing;
    static_cast<void>(editing.controller().apply(application::VisibleLines{30}));
    static_cast<void>(editing.controller().apply(application::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(editing.controller().apply(application::StoreVimRegister{
        'a', core::VimRegister{input, core::VimRegisterKind::characters}}));
    vim_characters(editing, U"i");
    const auto result = vim_inserted(editing, insert_count, timing);
    if (!result.has_value())
    {
        return std::unexpected(result.error());
    }
    vim_escape(editing);
    vim_characters(editing, U"\"ap");
    const std::string pasted = std::string(insert_count, 'x') + input;
    if (!saved_equals(editing, pasted))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto pasted_checksum = checksum_of(editing.files().written());
    vim_characters(editing, U"u");
    if (!saved_equals(editing, std::string(insert_count, 'x')))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return result.value() ^ pasted_checksum;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_inserted_while_recording(const std::string &input, application::TimingPort &timing)
{
    Editing editing;
    static_cast<void>(editing.controller().apply(application::VisibleLines{30}));
    static_cast<void>(editing.controller().apply(application::SelectEditMode{core::EditMode::vim}));
    vim_characters(editing, U"qai");
    const auto result = vim_inserted(editing, input.size(), timing);
    if (!result.has_value() || editing.files().written() != input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    vim_escape(editing);
    vim_characters(editing, U"qu");
    if (!saved_equals(editing, ""))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    vim_characters(editing, U"@a");
    if (!saved_equals(editing, input))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(editing.files().written());
}
} // namespace

std::uint64_t checksum_of(std::string_view bytes) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char byte : bytes)
    {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

namespace
{
[[nodiscard]] std::string large_input(std::size_t lines)
{
    constexpr std::string_view filler = "nenenib speed sample line for the open-large-file bench ";
    std::string input;
    input.reserve(lines * large_line_bytes);
    for (std::size_t line = 0; line < lines; ++line)
    {
        input += (std::format("{:06d} ", line) + std::string(filler) + std::string(filler))
                     .substr(0, large_line_bytes - 2);
        input += "\r\n";
    }
    return input;
}

[[nodiscard]] std::string japanese_line()
{
    std::string line = "000000 ";
    for (std::size_t repeat = 0; repeat < 90; ++repeat)
    {
        line += "日本語の長い行と分割表示の文字組みを測定する。";
    }
    return line;
}

[[nodiscard]] std::string japanese_input()
{
    const std::string line = japanese_line();
    std::string input;
    input.reserve((line.size() + 2) * 1000);
    for (std::size_t repeat = 0; repeat < 1000; ++repeat)
    {
        input += line;
        input += "\r\n";
    }
    return input;
}
} // namespace

namespace
{
[[nodiscard]] std::string large_probe_input(ProbeWorkload)
{
    return large_input(large_lines);
}

[[nodiscard]] std::string deleted_small_input(ProbeWorkload)
{
    return large_input((1024U * 1024U) / large_line_bytes);
}

[[nodiscard]] std::string insert_probe_input(ProbeWorkload)
{
    return std::string(insert_count, 'a');
}

[[nodiscard]] std::string display_probe_input(ProbeWorkload)
{
    return japanese_line();
}

[[nodiscard]] std::string validation_japanese_input(ProbeWorkload)
{
    return japanese_input();
}

[[nodiscard]] std::string register_small_input(ProbeWorkload)
{
    return std::string(1048576U, 'r');
}

[[nodiscard]] std::string register_large_input(ProbeWorkload)
{
    return std::string(16777216U, 'r');
}

[[nodiscard]] std::string record_small_input(ProbeWorkload)
{
    return std::string(200U, 'x');
}

[[nodiscard]] std::string record_large_input(ProbeWorkload)
{
    return std::string(2000U, 'x');
}

[[nodiscard]] std::string crlf_probe_input(ProbeWorkload)
{
    return crlf_buffer_input();
}

[[nodiscard]] std::string position_probe_input(ProbeWorkload)
{
    return position_buffer_input();
}

[[nodiscard]] std::string search_probe_input(ProbeWorkload)
{
    return search_buffer_input();
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_opened(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return opened(input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_buffered(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return buffered(input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_inserted(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return inserted(input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_displayed(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return displayed(input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_after_delete(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return inserted_after_delete(input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_validated_ascii(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return validated(input, 16800000U, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_validated_japanese(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return validated(input, 2079000U, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_registered(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return vim_inserted_with_register(input, timing);
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_recorded(ProbeWorkload, const std::string &input, application::TimingPort &timing)
{
    return vim_inserted_while_recording(input, timing);
}

[[nodiscard]] constexpr std::size_t checked_index(ProbeWorkload workload)
{
    switch (workload)
    {
    case ProbeWorkload::controller_open:
    case ProbeWorkload::buffer_create:
    case ProbeWorkload::controller_insert:
    case ProbeWorkload::display_long:
    case ProbeWorkload::insert_after_delete_small:
    case ProbeWorkload::insert_after_delete_large:
    case ProbeWorkload::validate_ascii:
    case ProbeWorkload::validate_japanese:
    case ProbeWorkload::vim_register_small:
    case ProbeWorkload::vim_register_large:
    case ProbeWorkload::vim_record_small:
    case ProbeWorkload::vim_record_large:
    case ProbeWorkload::scattered_crlf_lines:
    case ProbeWorkload::scattered_lf_lines:
    case ProbeWorkload::long_position:
    case ProbeWorkload::scattered_position:
    case ProbeWorkload::listed_name:
    case ProbeWorkload::listed_location:
    case ProbeWorkload::palette_narrow:
    case ProbeWorkload::palette_left:
    case ProbeWorkload::codepage_japanese:
    case ProbeWorkload::utf16_japanese:
    case ProbeWorkload::utf16_ascii:
    case ProbeWorkload::utf16_supplementary:
    case ProbeWorkload::controller_save:
    case ProbeWorkload::controller_save_bom:
    case ProbeWorkload::erase_scattered_head:
    case ProbeWorkload::erase_scattered_middle:
    case ProbeWorkload::erase_scattered_tail:
    case ProbeWorkload::erase_scattered_all:
    case ProbeWorkload::erase_single_middle:
    case ProbeWorkload::offset_long_head:
    case ProbeWorkload::offset_long_middle:
    case ProbeWorkload::offset_long_end:
    case ProbeWorkload::offset_scattered_middle:
    case ProbeWorkload::search_forward_head:
    case ProbeWorkload::search_forward_middle:
    case ProbeWorkload::search_forward_tail:
    case ProbeWorkload::search_backward_head:
    case ProbeWorkload::search_backward_middle:
    case ProbeWorkload::search_backward_tail:
    case ProbeWorkload::pattern_star_small:
    case ProbeWorkload::pattern_star_middle:
    case ProbeWorkload::pattern_star_large:
    case ProbeWorkload::pattern_multistar:
    case ProbeWorkload::pattern_greedy:
    case ProbeWorkload::pattern_literal_tail:
    case ProbeWorkload::pattern_literal_long:
    case ProbeWorkload::frame_dense_ascii:
    case ProbeWorkload::frame_dense_mixed:
    case ProbeWorkload::frame_sparse_tail:
    case ProbeWorkload::frame_selection:
    case ProbeWorkload::frame_search_visual:
        return static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(FrameRowsWorkload workload)
{
    switch (workload)
    {
    case FrameRowsWorkload::frame_rows_empty:
    case FrameRowsWorkload::frame_rows_short:
    case FrameRowsWorkload::frame_rows_thirty:
    case FrameRowsWorkload::frame_rows_full:
        return 53U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(SearchSnapshotWorkload workload)
{
    switch (workload)
    {
    case SearchSnapshotWorkload::frame_short:
    case SearchSnapshotWorkload::frame_long:
    case SearchSnapshotWorkload::repeat_long:
    case SearchSnapshotWorkload::retained_long:
    case SearchSnapshotWorkload::retained_short:
    case SearchSnapshotWorkload::unsearched:
    case SearchSnapshotWorkload::commit_short:
    case SearchSnapshotWorkload::typing_frame_short:
        return 57U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(PreviewCaretWorkload workload)
{
    switch (workload)
    {
    case PreviewCaretWorkload::long_thirty:
    case PreviewCaretWorkload::long_full:
    case PreviewCaretWorkload::short_thirty:
    case PreviewCaretWorkload::confirmed:
    case PreviewCaretWorkload::unsearched:
    case PreviewCaretWorkload::absent:
    case PreviewCaretWorkload::disabled:
        return 65U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(FrameDocumentWorkload workload)
{
    switch (workload)
    {
    case FrameDocumentWorkload::short_saved:
    case FrameDocumentWorkload::long_saved:
    case FrameDocumentWorkload::long_failed:
    case FrameDocumentWorkload::untitled:
        return 72U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(StatusItemsWorkload workload)
{
    switch (workload)
    {
    case StatusItemsWorkload::utf8_crlf:
    case StatusItemsWorkload::bom_lf:
    case StatusItemsWorkload::sjis_crlf:
    case StatusItemsWorkload::max_utf8_lf:
    case StatusItemsWorkload::large_bom_crlf:
    case StatusItemsWorkload::sjis_lf:
        return 76U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(FileNameWorkload workload)
{
    switch (workload)
    {
    case FileNameWorkload::windows:
    case FileNameWorkload::deep_ascii:
    case FileNameWorkload::mixed_utf8:
    case FileNameWorkload::bare_short:
    case FileNameWorkload::bare_long:
    case FileNameWorkload::trailing:
    case FileNameWorkload::root:
        return 82U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(TabDisplayWorkload workload)
{
    switch (workload)
    {
    case TabDisplayWorkload::ordinary_saved:
    case TabDisplayWorkload::deep_saved:
    case TabDisplayWorkload::wide_saved:
    case TabDisplayWorkload::short_saved:
    case TabDisplayWorkload::exact_saved:
    case TabDisplayWorkload::modified_boundary:
    case TabDisplayWorkload::folder_wide:
    case TabDisplayWorkload::root_saved:
    case TabDisplayWorkload::untitled_modified:
        return 89U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(CommandInputWorkload workload)
{
    switch (workload)
    {
    case CommandInputWorkload::search_long:
    case CommandInputWorkload::ex_long:
    case CommandInputWorkload::palette_long:
    case CommandInputWorkload::search_short:
    case CommandInputWorkload::ex_short:
    case CommandInputWorkload::palette_short:
    case CommandInputWorkload::ex_left_long:
    case CommandInputWorkload::palette_fill_long:
    case CommandInputWorkload::controller_search:
    case CommandInputWorkload::controller_ex:
    case CommandInputWorkload::controller_palette:
    case CommandInputWorkload::controller_fill:
        return 98U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(ExCompletionWorkload workload)
{
    switch (workload)
    {
    case ExCompletionWorkload::option:
    case ExCompletionWorkload::miss:
    case ExCompletionWorkload::custom_narrow:
    case ExCompletionWorkload::empty:
    case ExCompletionWorkload::set_wide:
    case ExCompletionWorkload::theme_wide:
    case ExCompletionWorkload::all_builtins:
    case ExCompletionWorkload::custom_wide:
    case ExCompletionWorkload::all_custom:
    case ExCompletionWorkload::frame_ex_option:
    case ExCompletionWorkload::frame_ex_empty:
    case ExCompletionWorkload::frame_palette:
        return 110U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(CompletionPrefixWorkload workload)
{
    switch (workload)
    {
    case CompletionPrefixWorkload::long_miss:
    case CompletionPrefixWorkload::theme_raw:
    case CompletionPrefixWorkload::theme_seeded:
    case CompletionPrefixWorkload::short_raw:
    case CompletionPrefixWorkload::short_seeded:
    case CompletionPrefixWorkload::empty_raw:
    case CompletionPrefixWorkload::empty_seeded:
        return 122U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(PaletteChoiceWorkload workload)
{
    switch (workload)
    {
    case PaletteChoiceWorkload::builtin_all:
    case PaletteChoiceWorkload::custom_all:
    case PaletteChoiceWorkload::miss:
    case PaletteChoiceWorkload::short_query:
    case PaletteChoiceWorkload::theme:
    case PaletteChoiceWorkload::direct_fill:
    case PaletteChoiceWorkload::direct_long:
    case PaletteChoiceWorkload::fallback_long:
    case PaletteChoiceWorkload::custom_narrow:
        return 129U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(OperationChoiceWorkload workload)
{
    switch (workload)
    {
    case OperationChoiceWorkload::reading_ordinary:
    case OperationChoiceWorkload::miss_ordinary:
    case OperationChoiceWorkload::all_ordinary:
    case OperationChoiceWorkload::all_vim:
    case OperationChoiceWorkload::space_ordinary:
    case OperationChoiceWorkload::key_vim:
    case OperationChoiceWorkload::name_ordinary:
    case OperationChoiceWorkload::close_key_vim:
    case OperationChoiceWorkload::description_miss:
    case OperationChoiceWorkload::short_name_miss:
        return 138U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(SearchSpanWorkload workload)
{
    switch (workload)
    {
    case SearchSpanWorkload::miss_16mib:
    case SearchSpanWorkload::next_16mib:
        return 148U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(TextObjectWorkload workload)
{
    switch (workload)
    {
    case TextObjectWorkload::sentence_giant:
    case TextObjectWorkload::sentence_dense:
    case TextObjectWorkload::tag_nested:
    case TextObjectWorkload::tag_unclosed:
        return 150U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(MarkdownWorkload workload)
{
    switch (workload)
    {
    case MarkdownWorkload::mixed_16mib:
    case MarkdownWorkload::mixed_16kib:
    case MarkdownWorkload::mixed_256kib:
    case MarkdownWorkload::mixed_4mib:
        return 154U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(NewlineIndexWorkload workload)
{
    switch (workload)
    {
    case NewlineIndexWorkload::payload_all_lf:
    case NewlineIndexWorkload::payload_prose:
    case NewlineIndexWorkload::payload_one_line:
    case NewlineIndexWorkload::lengths_all_lf:
        return 158U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t checked_index(LexerWorkload workload)
{
    switch (workload)
    {
    case LexerWorkload::cpp_visible_60:
    case LexerWorkload::cpp_backfill_200:
    case LexerWorkload::json_visible_60:
    case LexerWorkload::json_backfill_200:
    case LexerWorkload::python_visible_60:
    case LexerWorkload::python_backfill_200:
    case LexerWorkload::markdown_visible_60:
    case LexerWorkload::markdown_backfill_200:
    case LexerWorkload::javascript_visible_60:
    case LexerWorkload::javascript_backfill_200:
    case LexerWorkload::markdown_worst_line:
    case LexerWorkload::html_visible_60:
    case LexerWorkload::html_backfill_200:
    case LexerWorkload::xml_visible_60:
    case LexerWorkload::css_visible_60:
    case LexerWorkload::yaml_visible_60:
    case LexerWorkload::yaml_backfill_200:
    case LexerWorkload::toml_visible_60:
    case LexerWorkload::toml_backfill_200:
    case LexerWorkload::gdscript_visible_60:
    case LexerWorkload::gdscript_backfill_200:
    case LexerWorkload::dosini_visible_60:
    case LexerWorkload::gdresource_visible_60:
    case LexerWorkload::gdresource_backfill_200:
    case LexerWorkload::cs_visible_60:
    case LexerWorkload::cs_backfill_200:
    case LexerWorkload::rust_visible_60:
    case LexerWorkload::rust_backfill_200:
    case LexerWorkload::go_visible_60:
    case LexerWorkload::go_backfill_200:
    case LexerWorkload::php_visible_60:
    case LexerWorkload::php_backfill_200:
    case LexerWorkload::sql_visible_60:
    case LexerWorkload::lua_visible_60:
    case LexerWorkload::lua_backfill_200:
    case LexerWorkload::dockerfile_visible_60:
    case LexerWorkload::powershell_visible_60:
    case LexerWorkload::powershell_backfill_200:
    case LexerWorkload::shell_visible_60:
    case LexerWorkload::shell_backfill_200:
    case LexerWorkload::cmake_visible_60:
    case LexerWorkload::cmake_backfill_200:
        return 162U + static_cast<std::size_t>(workload);
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::size_t selection_index(ProbeSelection workload)
{
    return std::visit([](auto value) { return checked_index(value); }, workload);
}

[[nodiscard]] constexpr std::string_view dispatch_name(const ProbeDispatch &dispatch)
{
    return std::visit([](const auto &row) { return row.name; }, dispatch);
}

[[nodiscard]] ProbeSelection dispatch_workload(const ProbeDispatch &dispatch)
{
    return std::visit([](const auto &row) -> ProbeSelection { return row.workload; }, dispatch);
}

constexpr std::array<ProbeDispatch, 204> dispatches{{
    ProbeDispatchRow{ProbeWorkload::controller_open, "controller-open-utf8-16mib",
                     large_probe_input, run_opened},
    ProbeDispatchRow{ProbeWorkload::buffer_create, "buffer-from-utf8-16mib", large_probe_input,
                     run_buffered},
    ProbeDispatchRow{ProbeWorkload::controller_insert, "controller-insert-200", insert_probe_input,
                     run_inserted},
    ProbeDispatchRow{ProbeWorkload::display_long, "display-line-long", display_probe_input,
                     run_displayed},
    ProbeDispatchRow{ProbeWorkload::insert_after_delete_small,
                     "controller-insert-200-after-delete-1mib", deleted_small_input,
                     run_after_delete},
    ProbeDispatchRow{ProbeWorkload::insert_after_delete_large,
                     "controller-insert-200-after-delete-16mib", large_probe_input,
                     run_after_delete},
    ProbeDispatchRow{ProbeWorkload::validate_ascii, "utf8-validate-ascii-16mib", large_probe_input,
                     run_validated_ascii},
    ProbeDispatchRow{ProbeWorkload::validate_japanese, "utf8-validate-japanese-6mib",
                     validation_japanese_input, run_validated_japanese},
    ProbeDispatchRow{ProbeWorkload::vim_register_small, "controller-vim-insert-200-register-1mib",
                     register_small_input, run_registered},
    ProbeDispatchRow{ProbeWorkload::vim_register_large, "controller-vim-insert-200-register-16mib",
                     register_large_input, run_registered},
    ProbeDispatchRow{ProbeWorkload::vim_record_small, "controller-vim-record-insert-200",
                     record_small_input, run_recorded},
    ProbeDispatchRow{ProbeWorkload::vim_record_large, "controller-vim-record-insert-2000",
                     record_large_input, run_recorded},
    ProbeDispatchRow{ProbeWorkload::scattered_crlf_lines, "buffer-line-text-scattered-crlf-4096",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::scattered_lf_lines, "buffer-line-text-scattered-lf-4096",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::long_position, "buffer-position-long-utf8-57344",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::scattered_position, "buffer-position-scattered-utf8-57344",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::listed_name, "palette-listed-name-5000", scoped_input_of,
                     run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::listed_location, "palette-listed-location-5000",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::palette_narrow, "palette-append-narrow-5000-to-50",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::palette_left, "palette-caret-left-5000", scoped_input_of,
                     run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::codepage_japanese, "codepage-to-utf8-cp932-japanese-16mib",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::utf16_japanese, "utf16-to-utf8-japanese-8m-units",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::utf16_ascii, "utf16-to-utf8-ascii-8m-units", scoped_input_of,
                     run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::utf16_supplementary, "utf16-to-utf8-supplementary-8m-units",
                     scoped_input_of, run_scoped_workload},
    ProbeDispatchRow{ProbeWorkload::controller_save, "controller-save-utf8-16mib",
                     large_probe_input, saved},
    ProbeDispatchRow{ProbeWorkload::controller_save_bom, "controller-save-utf8-bom-16mib",
                     large_probe_input, saved},
    ProbeDispatchRow{ProbeWorkload::erase_scattered_head, "buffer-erase-scattered-head-4096",
                     crlf_probe_input, erased_buffer},
    ProbeDispatchRow{ProbeWorkload::erase_scattered_middle, "buffer-erase-scattered-middle-4096",
                     crlf_probe_input, erased_buffer},
    ProbeDispatchRow{ProbeWorkload::erase_scattered_tail, "buffer-erase-scattered-tail-4096",
                     crlf_probe_input, erased_buffer},
    ProbeDispatchRow{ProbeWorkload::erase_scattered_all, "buffer-erase-scattered-all-4096",
                     crlf_probe_input, erased_buffer},
    ProbeDispatchRow{ProbeWorkload::erase_single_middle, "buffer-erase-single-middle-4096",
                     crlf_probe_input, erased_buffer},
    ProbeDispatchRow{ProbeWorkload::offset_long_head, "buffer-offset-long-head-57344",
                     position_probe_input, queried_offset},
    ProbeDispatchRow{ProbeWorkload::offset_long_middle, "buffer-offset-long-middle-57344",
                     position_probe_input, queried_offset},
    ProbeDispatchRow{ProbeWorkload::offset_long_end, "buffer-offset-long-end-57344",
                     position_probe_input, queried_offset},
    ProbeDispatchRow{ProbeWorkload::offset_scattered_middle, "buffer-offset-scattered-middle-57344",
                     position_probe_input, queried_offset},
    ProbeDispatchRow{ProbeWorkload::search_forward_head, "search-forward-head-many-4096",
                     search_probe_input, searched_buffer},
    ProbeDispatchRow{ProbeWorkload::search_forward_middle, "search-forward-middle-many-4096",
                     search_probe_input, searched_buffer},
    ProbeDispatchRow{ProbeWorkload::search_forward_tail, "search-forward-tail-many-4096",
                     search_probe_input, searched_buffer},
    ProbeDispatchRow{ProbeWorkload::search_backward_head, "search-backward-head-many-4096",
                     search_probe_input, searched_buffer},
    ProbeDispatchRow{ProbeWorkload::search_backward_middle, "search-backward-middle-many-4096",
                     search_probe_input, searched_buffer},
    ProbeDispatchRow{ProbeWorkload::search_backward_tail, "search-backward-tail-many-4096",
                     search_probe_input, searched_buffer},
    ProbeDispatchRow{ProbeWorkload::pattern_star_small, "pattern-star-miss-1024",
                     pattern_probe_input, matched_pattern},
    ProbeDispatchRow{ProbeWorkload::pattern_star_middle, "pattern-star-miss-2048",
                     pattern_probe_input, matched_pattern},
    ProbeDispatchRow{ProbeWorkload::pattern_star_large, "pattern-star-miss-4096",
                     pattern_probe_input, matched_pattern},
    ProbeDispatchRow{ProbeWorkload::pattern_multistar, "pattern-multistar-miss-32",
                     pattern_probe_input, matched_pattern},
    ProbeDispatchRow{ProbeWorkload::pattern_greedy, "pattern-greedy-hit-4096", pattern_probe_input,
                     matched_pattern},
    ProbeDispatchRow{ProbeWorkload::pattern_literal_tail, "pattern-literal-tail-4102",
                     pattern_probe_input, matched_pattern},
    ProbeDispatchRow{ProbeWorkload::pattern_literal_long, "pattern-literal-long-4096",
                     pattern_probe_input, matched_pattern},
    ProbeDispatchRow{ProbeWorkload::frame_dense_ascii, "frame-search-dense-ascii-4096",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_dense_mixed, "frame-search-dense-mixed-2340",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_sparse_tail, "frame-search-sparse-tail-16384",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_selection, "frame-selection-ascii-16384",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_search_visual, "frame-search-visual-ascii-4096",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{FrameRowsWorkload::frame_rows_empty, "frame-rows-empty-64", frame_rows_input,
                     framed_rows},
    ProbeDispatchRow{FrameRowsWorkload::frame_rows_short, "frame-rows-short-64", frame_rows_input,
                     framed_rows},
    ProbeDispatchRow{FrameRowsWorkload::frame_rows_thirty, "frame-rows-30-64", frame_rows_input,
                     framed_rows},
    ProbeDispatchRow{FrameRowsWorkload::frame_rows_full, "frame-rows-120-64", frame_rows_input,
                     framed_rows},
    ProbeDispatchRow{SearchSnapshotWorkload::frame_short, "search-snapshot-frame-short-64",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::frame_long, "search-snapshot-frame-long-64",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::repeat_long, "search-snapshot-repeat-long-64",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::retained_long, "search-snapshot-retained-long-200",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::retained_short, "search-snapshot-retained-short-200",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::unsearched, "search-snapshot-unsearched-200",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::commit_short, "search-snapshot-commit-short-64",
                     search_snapshot_input, probed_search_snapshot},
    ProbeDispatchRow{SearchSnapshotWorkload::typing_frame_short,
                     "search-snapshot-typing-frame-short-64", search_snapshot_input,
                     probed_search_snapshot},
    ProbeDispatchRow{PreviewCaretWorkload::long_thirty, "preview-caret-long-30-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{PreviewCaretWorkload::long_full, "preview-caret-long-120-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{PreviewCaretWorkload::short_thirty, "preview-caret-short-30-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{PreviewCaretWorkload::confirmed, "preview-caret-confirmed-30-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{PreviewCaretWorkload::unsearched, "preview-caret-unsearched-30-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{PreviewCaretWorkload::absent, "preview-caret-absent-30-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{PreviewCaretWorkload::disabled, "preview-caret-disabled-30-64",
                     preview_caret_input, probed_preview_caret},
    ProbeDispatchRow{FrameDocumentWorkload::short_saved, "frame-document-short-saved-256",
                     frame_document_input, framed_documents},
    ProbeDispatchRow{FrameDocumentWorkload::long_saved, "frame-document-long-saved-256",
                     frame_document_input, framed_documents},
    ProbeDispatchRow{FrameDocumentWorkload::long_failed, "frame-document-long-failed-256",
                     frame_document_input, framed_documents},
    ProbeDispatchRow{FrameDocumentWorkload::untitled, "frame-document-untitled-256",
                     frame_document_input, framed_documents},
    ProbeDispatchRow{StatusItemsWorkload::utf8_crlf, "status-items-utf8-crlf-1024",
                     status_items_input, fixed_status_items},
    ProbeDispatchRow{StatusItemsWorkload::bom_lf, "status-items-bom-lf-1024", status_items_input,
                     fixed_status_items},
    ProbeDispatchRow{StatusItemsWorkload::sjis_crlf, "status-items-sjis-crlf-1024",
                     status_items_input, fixed_status_items},
    ProbeDispatchRow{StatusItemsWorkload::max_utf8_lf, "status-items-max-utf8-lf-1024",
                     status_items_input, fixed_status_items},
    ProbeDispatchRow{StatusItemsWorkload::large_bom_crlf, "status-items-large-bom-crlf-1024",
                     status_items_input, fixed_status_items},
    ProbeDispatchRow{StatusItemsWorkload::sjis_lf, "status-items-sjis-lf-1024", status_items_input,
                     fixed_status_items},
    ProbeDispatchRow{FileNameWorkload::windows, "file-name-windows-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{FileNameWorkload::deep_ascii, "file-name-deep-ascii-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{FileNameWorkload::mixed_utf8, "file-name-mixed-utf8-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{FileNameWorkload::bare_short, "file-name-bare-short-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{FileNameWorkload::bare_long, "file-name-bare-long-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{FileNameWorkload::trailing, "file-name-trailing-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{FileNameWorkload::root, "file-name-root-4096", file_name_input,
                     fixed_file_name},
    ProbeDispatchRow{TabDisplayWorkload::ordinary_saved, "tab-display-ordinary-saved-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::deep_saved, "tab-display-deep-saved-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::wide_saved, "tab-display-wide-saved-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::short_saved, "tab-display-short-saved-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::exact_saved, "tab-display-exact-saved-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::modified_boundary, "tab-display-modified-boundary-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::folder_wide, "tab-display-folder-wide-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::root_saved, "tab-display-root-saved-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{TabDisplayWorkload::untitled_modified, "tab-display-untitled-modified-1024",
                     tab_display_input, fixed_tab_display},
    ProbeDispatchRow{CommandInputWorkload::search_long, "command-input-search-long-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::ex_long, "command-input-ex-long-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::palette_long, "command-input-palette-long-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::search_short, "command-input-search-short-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::ex_short, "command-input-ex-short-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::palette_short, "command-input-palette-short-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::ex_left_long, "command-input-ex-left-long-1024",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::palette_fill_long,
                     "command-input-palette-fill-long-1024", command_input_input,
                     probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::controller_search, "command-input-controller-search-64",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::controller_ex, "command-input-controller-ex-64",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::controller_palette,
                     "command-input-controller-palette-64", command_input_input,
                     probed_command_input},
    ProbeDispatchRow{CommandInputWorkload::controller_fill, "command-input-controller-fill-64",
                     command_input_input, probed_command_input},
    ProbeDispatchRow{ExCompletionWorkload::option, "ex-completion-option-1024", ex_completion_input,
                     probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::miss, "ex-completion-miss-1024", ex_completion_input,
                     probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::custom_narrow, "ex-completion-custom-narrow-128",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::empty, "ex-completion-empty-1024", ex_completion_input,
                     probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::set_wide, "ex-completion-set-wide-1024",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::theme_wide, "ex-completion-theme-wide-1024",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::all_builtins, "ex-completion-all-builtins-1024",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::custom_wide, "ex-completion-custom-wide-128",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::all_custom, "ex-completion-all-custom-128",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::frame_ex_option, "ex-completion-frame-ex-option-64",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::frame_ex_empty, "ex-completion-frame-ex-empty-64",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{ExCompletionWorkload::frame_palette, "ex-completion-frame-palette-64",
                     ex_completion_input, probed_ex_completion},
    ProbeDispatchRow{CompletionPrefixWorkload::long_miss, "completion-prefix-long-miss-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{CompletionPrefixWorkload::theme_raw, "completion-prefix-theme-raw-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{CompletionPrefixWorkload::theme_seeded, "completion-prefix-theme-seeded-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{CompletionPrefixWorkload::short_raw, "completion-prefix-short-raw-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{CompletionPrefixWorkload::short_seeded, "completion-prefix-short-seeded-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{CompletionPrefixWorkload::empty_raw, "completion-prefix-empty-raw-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{CompletionPrefixWorkload::empty_seeded, "completion-prefix-empty-seeded-4096",
                     completion_prefix_input, probed_completion_prefix},
    ProbeDispatchRow{PaletteChoiceWorkload::builtin_all, "palette-choice-builtin-all-512",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::custom_all, "palette-choice-custom-all-128",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::miss, "palette-choice-miss-1024", palette_choice_input,
                     probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::short_query, "palette-choice-short-1024",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::theme, "palette-choice-theme-512", palette_choice_input,
                     probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::direct_fill, "palette-choice-direct-fill-1024",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::direct_long, "palette-choice-direct-long-1024",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::fallback_long, "palette-choice-fallback-long-512",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{PaletteChoiceWorkload::custom_narrow, "palette-choice-custom-narrow-128",
                     palette_choice_input, probed_palette_choice},
    ProbeDispatchRow{OperationChoiceWorkload::reading_ordinary,
                     "operation-choice-reading-ordinary-1024", operation_choice_input,
                     probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::miss_ordinary, "operation-choice-miss-ordinary-1024",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::all_ordinary, "operation-choice-all-ordinary-256",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::all_vim, "operation-choice-all-vim-256",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::space_ordinary, "operation-choice-space-ordinary-256",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::key_vim, "operation-choice-key-vim-1024",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::name_ordinary, "operation-choice-name-ordinary-1024",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::close_key_vim, "operation-choice-close-key-vim-1024",
                     operation_choice_input, probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::description_miss,
                     "operation-choice-description-miss-1024", operation_choice_input,
                     probed_operation_choice},
    ProbeDispatchRow{OperationChoiceWorkload::short_name_miss,
                     "operation-choice-short-name-miss-1024", operation_choice_input,
                     probed_operation_choice},
    ProbeDispatchRow{SearchSpanWorkload::miss_16mib, "search-miss-16mib", search_span_input,
                     searched_spans},
    ProbeDispatchRow{SearchSpanWorkload::next_16mib, "search-next-16mib", search_span_input,
                     searched_spans},
    ProbeDispatchRow{TextObjectWorkload::sentence_giant, "text-object-sentence-giant-1line-16384",
                     text_object_input, text_object_ranges},
    ProbeDispatchRow{TextObjectWorkload::sentence_dense, "text-object-sentence-dense-4096",
                     text_object_input, text_object_ranges},
    ProbeDispatchRow{TextObjectWorkload::tag_nested, "text-object-tag-nested-4096",
                     text_object_input, text_object_ranges},
    ProbeDispatchRow{TextObjectWorkload::tag_unclosed, "text-object-tag-unclosed-4096",
                     text_object_input, text_object_ranges},
    ProbeDispatchRow{MarkdownWorkload::mixed_16mib, "markdown-parse-mixed-16mib", markdown_input,
                     parsed_markdown},
    ProbeDispatchRow{MarkdownWorkload::mixed_16kib, "markdown-parse-mixed-16kib", markdown_input,
                     parsed_markdown},
    ProbeDispatchRow{MarkdownWorkload::mixed_256kib, "markdown-parse-mixed-256kib", markdown_input,
                     parsed_markdown},
    ProbeDispatchRow{MarkdownWorkload::mixed_4mib, "markdown-parse-mixed-4mib", markdown_input,
                     parsed_markdown},
    ProbeDispatchRow{NewlineIndexWorkload::payload_all_lf, "newline-index-payload-all-lf-16mib",
                     newline_index_input, indexed_payload},
    ProbeDispatchRow{NewlineIndexWorkload::payload_prose, "newline-index-payload-prose-16mib",
                     newline_index_input, indexed_payload},
    ProbeDispatchRow{NewlineIndexWorkload::payload_one_line, "newline-index-payload-one-line-16mib",
                     newline_index_input, indexed_payload},
    ProbeDispatchRow{NewlineIndexWorkload::lengths_all_lf, "newline-index-lengths-all-lf-16mib",
                     newline_index_input, indexed_lengths},
    ProbeDispatchRow{LexerWorkload::cpp_visible_60, "lexer-cpp-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::cpp_backfill_200, "lexer-cpp-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::json_visible_60, "lexer-json-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::json_backfill_200, "lexer-json-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::python_visible_60, "lexer-python-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::python_backfill_200, "lexer-python-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::markdown_visible_60, "lexer-markdown-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::markdown_backfill_200, "lexer-markdown-backfill-200",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::javascript_visible_60, "lexer-javascript-visible-60",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::javascript_backfill_200, "lexer-javascript-backfill-200",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::markdown_worst_line, "lexer-markdown-worst-line", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::html_visible_60, "lexer-html-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::html_backfill_200, "lexer-html-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::xml_visible_60, "lexer-xml-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::css_visible_60, "lexer-css-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::yaml_visible_60, "lexer-yaml-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::yaml_backfill_200, "lexer-yaml-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::toml_visible_60, "lexer-toml-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::toml_backfill_200, "lexer-toml-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::gdscript_visible_60, "lexer-gdscript-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::gdscript_backfill_200, "lexer-gdscript-backfill-200",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::dosini_visible_60, "lexer-dosini-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::gdresource_visible_60, "lexer-gdresource-visible-60",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::gdresource_backfill_200, "lexer-gdresource-backfill-200",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::cs_visible_60, "lexer-cs-visible-60", lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::cs_backfill_200, "lexer-cs-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::rust_visible_60, "lexer-rust-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::rust_backfill_200, "lexer-rust-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::go_visible_60, "lexer-go-visible-60", lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::go_backfill_200, "lexer-go-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::php_visible_60, "lexer-php-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::php_backfill_200, "lexer-php-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::sql_visible_60, "lexer-sql-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::lua_visible_60, "lexer-lua-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::lua_backfill_200, "lexer-lua-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::dockerfile_visible_60, "lexer-dockerfile-visible-60",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::powershell_visible_60, "lexer-powershell-visible-60",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::powershell_backfill_200, "lexer-powershell-backfill-200",
                     lexer_input, lexed_lines},
    ProbeDispatchRow{LexerWorkload::shell_visible_60, "lexer-shell-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::shell_backfill_200, "lexer-shell-backfill-200", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::cmake_visible_60, "lexer-cmake-visible-60", lexer_input,
                     lexed_lines},
    ProbeDispatchRow{LexerWorkload::cmake_backfill_200, "lexer-cmake-backfill-200", lexer_input,
                     lexed_lines},
}};

constexpr auto dispatch_names = []
{
    std::array<std::string_view, dispatches.size()> names{};
    for (std::size_t index = 0; index < dispatches.size(); ++index)
    {
        names.at(index) = dispatch_name(dispatches.at(index));
    }
    return names;
}();

[[nodiscard]] constexpr bool name_is_unique(std::size_t index)
{
    for (std::size_t previous = 0; previous < index; ++previous)
    {
        if (dispatch_names[index] == dispatch_names[previous])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool dispatch_is_complete()
{
    for (std::size_t index = 0; index < dispatches.size(); ++index)
    {
        const bool valid = std::visit(
            [index](const auto &row)
            {
                return checked_index(row.workload) == index && !row.name.empty() &&
                       row.input != nullptr && row.run != nullptr;
            },
            dispatches[index]);
        if (!valid || !name_is_unique(index))
        {
            return false;
        }
    }
    return true;
}
static_assert(dispatches.size() == 204U);
static_assert(dispatch_is_complete());

} // namespace

std::optional<ProbeSelection> workload_of(std::string_view name) noexcept
{
    for (const auto &row : dispatches)
    {
        if (dispatch_name(row) == name)
        {
            return dispatch_workload(row);
        }
    }
    return std::nullopt;
}

std::string input_of(ProbeSelection workload)
{
    return std::visit([](const auto &row) { return row.input(row.workload); },
                      dispatches.at(selection_index(workload)));
}

std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeSelection workload, const std::string &input, application::TimingPort &timing)
{
    return std::visit([&input, &timing](const auto &row)
                      { return row.run(row.workload, input, timing); },
                      dispatches.at(selection_index(workload)));
}
} // namespace nenenib::tests::performance
