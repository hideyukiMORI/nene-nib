#include "ProbeWorkload.hpp"

#include "ScopedProbe.hpp"

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

std::optional<ProbeWorkload> workload_of(std::string_view name) noexcept
{
    constexpr std::array<std::pair<std::string_view, ProbeWorkload>, 24> names{{
        {"controller-open-utf8-16mib", ProbeWorkload::controller_open},
        {"buffer-from-utf8-16mib", ProbeWorkload::buffer_create},
        {"controller-insert-200", ProbeWorkload::controller_insert},
        {"display-line-long", ProbeWorkload::display_long},
        {"controller-insert-200-after-delete-1mib", ProbeWorkload::insert_after_delete_small},
        {"controller-insert-200-after-delete-16mib", ProbeWorkload::insert_after_delete_large},
        {"utf8-validate-ascii-16mib", ProbeWorkload::validate_ascii},
        {"utf8-validate-japanese-6mib", ProbeWorkload::validate_japanese},
        {"controller-vim-insert-200-register-1mib", ProbeWorkload::vim_register_small},
        {"controller-vim-insert-200-register-16mib", ProbeWorkload::vim_register_large},
        {"controller-vim-record-insert-200", ProbeWorkload::vim_record_small},
        {"controller-vim-record-insert-2000", ProbeWorkload::vim_record_large},
        {"buffer-line-text-scattered-crlf-4096", ProbeWorkload::scattered_crlf_lines},
        {"buffer-line-text-scattered-lf-4096", ProbeWorkload::scattered_lf_lines},
        {"buffer-position-long-utf8-57344", ProbeWorkload::long_position},
        {"buffer-position-scattered-utf8-57344", ProbeWorkload::scattered_position},
        {"palette-listed-name-5000", ProbeWorkload::listed_name},
        {"palette-listed-location-5000", ProbeWorkload::listed_location},
        {"palette-append-narrow-5000-to-50", ProbeWorkload::palette_narrow},
        {"palette-caret-left-5000", ProbeWorkload::palette_left},
        {"codepage-to-utf8-cp932-japanese-16mib", ProbeWorkload::codepage_japanese},
        {"utf16-to-utf8-japanese-8m-units", ProbeWorkload::utf16_japanese},
        {"utf16-to-utf8-ascii-8m-units", ProbeWorkload::utf16_ascii},
        {"utf16-to-utf8-supplementary-8m-units", ProbeWorkload::utf16_supplementary},
    }};
    for (const auto &[text, workload] : names)
    {
        if (name == text)
        {
            return workload;
        }
    }
    return std::nullopt;
}

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

std::string input_of(ProbeWorkload workload)
{
    switch (workload)
    {
    case ProbeWorkload::controller_open:
    case ProbeWorkload::buffer_create:
    case ProbeWorkload::insert_after_delete_large:
    case ProbeWorkload::validate_ascii:
        return large_input(large_lines);
    case ProbeWorkload::insert_after_delete_small:
        return large_input((1024U * 1024U) / large_line_bytes);
    case ProbeWorkload::controller_insert:
        return std::string(insert_count, 'a');
    case ProbeWorkload::display_long:
        return japanese_line();
    case ProbeWorkload::validate_japanese:
        return japanese_input();
    case ProbeWorkload::vim_register_small:
        return std::string(1048576U, 'r');
    case ProbeWorkload::vim_register_large:
        return std::string(16777216U, 'r');
    case ProbeWorkload::vim_record_small:
        return std::string(200U, 'x');
    case ProbeWorkload::vim_record_large:
        return std::string(2000U, 'x');
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
        return scoped_input_of(workload);
    }
    std::unreachable();
}

std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    switch (workload)
    {
    case ProbeWorkload::controller_open:
        return opened(input, timing);
    case ProbeWorkload::buffer_create:
        return buffered(input, timing);
    case ProbeWorkload::controller_insert:
        return inserted(input, timing);
    case ProbeWorkload::display_long:
        return displayed(input, timing);
    case ProbeWorkload::insert_after_delete_small:
    case ProbeWorkload::insert_after_delete_large:
        return inserted_after_delete(input, timing);
    case ProbeWorkload::validate_ascii:
        return validated(input, 16800000U, timing);
    case ProbeWorkload::validate_japanese:
        return validated(input, 2079000U, timing);
    case ProbeWorkload::vim_register_small:
    case ProbeWorkload::vim_register_large:
        return vim_inserted_with_register(input, timing);
    case ProbeWorkload::vim_record_small:
    case ProbeWorkload::vim_record_large:
        return vim_inserted_while_recording(input, timing);
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
        return run_scoped_workload(workload, input, timing);
    }
    std::unreachable();
}
} // namespace nenenib::tests::performance
