#include "ProbeWorkload.hpp"

#include "BufferProbe.hpp"
#include "FrameProbe.hpp"
#include "FrameRowsProbe.hpp"
#include "PatternProbe.hpp"
#include "ProbeDispatch.hpp"
#include "ProbeSelection.hpp"
#include "ScopedProbe.hpp"
#include "SearchProbe.hpp"
#include "SearchSnapshotProbe.hpp"

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

constexpr std::array<ProbeDispatch, 65> dispatches{{
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
    ProbeDispatchRow{ProbeWorkload::frame_dense_ascii, "frame-search-dense-ascii-8192",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_dense_mixed, "frame-search-dense-mixed-4096",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_sparse_tail, "frame-search-sparse-tail-32768",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_selection, "frame-selection-ascii-32768",
                     frame_probe_input, framed_line},
    ProbeDispatchRow{ProbeWorkload::frame_search_visual, "frame-search-visual-ascii-8192",
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
}};

[[nodiscard]] constexpr bool name_is_unique(std::size_t index)
{
    for (std::size_t previous = 0; previous < index; ++previous)
    {
        if (dispatch_name(dispatches[index]) == dispatch_name(dispatches[previous]))
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
static_assert(dispatches.size() == 65U);
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
