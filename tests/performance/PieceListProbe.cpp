#include "PieceListProbe.hpp"

#include "DocumentByteLimit.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "Milestone.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "SelectEditMode.hpp"
#include "SubstituteAll.hpp"
#include "TextBuffer.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests::performance
{
namespace
{
// 本文の行の数（read / replace / frame と、replace_all）。
constexpr std::size_t document_lines = 20000U;
constexpr std::size_t replace_all_lines = 100000U;
// read の区間: 真ん中の 40 行を 50 周。
constexpr std::size_t read_first_line = 10001U;
constexpr std::size_t read_lines = 40U;
constexpr std::size_t read_rounds = 50U;
// replace の区間の回数と、frame の区間の `l` `h` の組の数。
constexpr std::size_t replace_calls = 200U;
constexpr std::size_t timed_pairs = 100U;
// read の 1 行で引く位置（行頭から 3 バイト・ASCII の行なので code point も 3 つ）と範囲の長さ。
constexpr std::size_t read_column_bytes = 3U;
constexpr std::size_t read_range_bytes = 64U;

// 区間の外で作る piece の数の目安（実際は 2 × 挿入の数 + 1）。
[[nodiscard]] std::size_t pieces_of(PieceListWorkload workload) noexcept
{
    switch (workload)
    {
    case PieceListWorkload::reads_after_1:
    case PieceListWorkload::replaces_after_1:
    case PieceListWorkload::replace_all_after_1:
    case PieceListWorkload::frame_after_1:
        return 1U;
    case PieceListWorkload::reads_after_1000:
    case PieceListWorkload::replaces_after_1000:
        return 1000U;
    case PieceListWorkload::reads_after_4000:
    case PieceListWorkload::replaces_after_4000:
        return 4000U;
    case PieceListWorkload::replace_all_after_8000:
        return 8000U;
    case PieceListWorkload::reads_after_16000:
    case PieceListWorkload::replaces_after_16000:
    case PieceListWorkload::frame_after_16000:
        return 16000U;
    }
    std::unreachable();
}

[[nodiscard]] bool replaces_all(PieceListWorkload workload) noexcept
{
    return workload == PieceListWorkload::replace_all_after_1 ||
           workload == PieceListWorkload::replace_all_after_8000;
}

[[nodiscard]] std::string repeated_lines(std::string_view line, std::size_t count)
{
    std::string result;
    result.reserve(line.size() * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        result += line;
    }
    return result;
}

// 等間隔の行頭（2 行目から）へ `#` を P / 2 回入れる。1 回で original の piece が割れて 2
// つ増える。
[[nodiscard]] std::expected<core::TextBuffer, ProbeFailure>
scattered(const std::string &input, std::size_t lines, std::size_t pieces)
{
    const auto parsed = core::TextBuffer::from_utf8(input);
    if (!parsed.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    core::TextBuffer text = parsed.value();
    const std::size_t inserts = pieces / 2U;
    const std::size_t spacing = inserts == 0 ? 1U : lines / inserts;
    for (std::size_t insert = 0; insert < inserts; ++insert)
    {
        const core::LineNumber line{(insert * spacing) + 2U};
        text = text.insert(text.line_start(line), "#");
    }
    if (text.piece_count() != (2U * inserts) + 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::cerr << "preparedPieces=" << text.piece_count() << '\n';
    return text;
}

// 区間の後に、模型（本文を写した std::string の行）と 40 行の答えを照合する。
[[nodiscard]] bool reads_match(const core::TextBuffer &text)
{
    const std::string model = text.text();
    std::vector<std::size_t> starts{0};
    for (std::size_t at = 0; at < model.size(); ++at)
    {
        if (model[at] == '\n')
        {
            starts.push_back(at + 1U);
        }
    }
    bool same = true;
    for (std::size_t line = read_first_line; line < read_first_line + read_lines; ++line)
    {
        const std::size_t begin = starts.at(line - 1U);
        const std::size_t end = starts.at(line) - 1U;
        const core::LineNumber number{line};
        const core::Offset at{begin + read_column_bytes};
        same = same && text.line_start(number).value == begin &&
               text.line_end(number).value == end &&
               text.line_text(number) == model.substr(begin, end - begin) &&
               text.offset_of(text.position_of(at)).value == at.value &&
               text.text_range(core::Offset{begin}, core::Offset{begin + read_range_bytes}) ==
                   model.substr(begin, read_range_bytes);
    }
    return same;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_reads(PieceListWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto prepared = scattered(input, document_lines, pieces_of(workload));
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    const core::TextBuffer &text = prepared.value();
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t round = 0; round < read_rounds; ++round)
    {
        for (std::size_t line = read_first_line; line < read_first_line + read_lines; ++line)
        {
            const core::LineNumber number{line};
            const core::Offset start = text.line_start(number);
            const core::Offset end = text.line_end(number);
            const std::string content = text.line_text(number);
            const core::TextPosition position =
                text.position_of(core::Offset{start.value + read_column_bytes});
            const core::Offset back = text.offset_of(position);
            const std::string range =
                text.text_range(start, core::Offset{start.value + read_range_bytes});
            checksum += (end.value - start.value) + content.size() + position.column.value +
                        (back.value - start.value) + range.size();
        }
    }
    timing.mark(core::Milestone::probe_finished);
    if (!reads_match(text))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_replaces(PieceListWorkload workload, const std::string &input, application::TimingPort &timing)
{
    const auto prepared = scattered(input, document_lines, pieces_of(workload));
    if (!prepared.has_value())
    {
        return std::unexpected(prepared.error());
    }
    const core::TextBuffer &text = prepared.value();
    std::vector<std::size_t> offsets;
    offsets.reserve(replace_calls);
    for (std::size_t call = 0; call < replace_calls; ++call)
    {
        const core::LineNumber line{1U + ((call * 97U) % document_lines)};
        offsets.push_back(text.line_start(line).value + 2U);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (const std::size_t at : offsets)
    {
        const auto next = text.replaced(core::Offset{at}, core::Offset{at + 1U}, "Q");
        checksum += next.size_bytes() + next.piece_count();
    }
    timing.mark(core::Milestone::probe_finished);
    const auto sample =
        text.replaced(core::Offset{offsets.front()}, core::Offset{offsets.front() + 1U}, "Q");
    if (sample.size_bytes() != text.size_bytes() ||
        sample.text_range(core::Offset{offsets.front()}, core::Offset{offsets.front() + 1U}) != "Q")
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_replace_all(PieceListWorkload workload, const std::string &input,
                application::TimingPort &timing)
{
    const auto prepared = scattered(input, replace_all_lines, pieces_of(workload));
    const auto pattern = core::VimPattern::parse("foo", core::VimSearchDirection::forward,
                                                 core::VimPatternCase::sensitive);
    if (!prepared.has_value() || !pattern.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    timing.mark(core::Milestone::probe_started);
    const auto edit = core::substituted_all(prepared.value(), pattern.value(), "FOOO",
                                            core::maximum_document_bytes);
    timing.mark(core::Milestone::probe_finished);
    if (!edit.has_value())
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &inserted = edit.value().inserted;
    std::uint64_t replaced = 0;
    for (std::size_t at = inserted.find("FOOO"); at != std::string::npos;
         at = inserted.find("FOOO", at + 4U))
    {
        ++replaced;
    }
    if (replaced != replace_all_lines)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return replaced + inserted.size();
}

void pressed(application::EditorController &controller, core::VimKey key)
{
    static_cast<void>(controller.apply(application::VimKeyPress{key}));
}

void pressed_digits(application::EditorController &controller, std::size_t value)
{
    const std::string digits = std::to_string(value);
    for (const char digit : digits)
    {
        pressed(controller, core::VimCharacter{static_cast<char32_t>(digit)});
    }
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_frame(PieceListWorkload workload, const std::string &input, application::TimingPort &timing)
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(
        application::VisibleLines{40U, core::TextViewGeometry{1200, 40, 96, 10.0F}}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\pieces.txt").value()}));
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    const std::size_t rounds = pieces_of(workload) / 2U;
    for (std::size_t round = 0; round < rounds; ++round)
    {
        pressed(controller, core::VimCharacter{U'I'});
        pressed(controller, core::VimCharacter{U'#'});
        pressed(controller, core::VimSpecialKey::escape);
        pressed(controller, core::VimCharacter{U'j'});
    }
    // 編集の数に依らず、同じ行（編集した行の次）で区間を始める。
    pressed_digits(controller, rounds + 1U);
    pressed(controller, core::VimCharacter{U'G'});
    static_cast<void>(controller.frame());
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t pair = 0; pair < timed_pairs; ++pair)
    {
        pressed(controller, core::VimCharacter{U'l'});
        checksum += controller.frame().caret.position.column.value;
        pressed(controller, core::VimCharacter{U'h'});
        checksum += controller.frame().caret.position.column.value;
    }
    timing.mark(core::Milestone::probe_finished);
    const auto frame = controller.frame();
    if (checksum != timed_pairs * 3U || frame.caret.position.line.value != rounds + 1U ||
        frame.total_lines != document_lines + 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum + rounds;
}
} // namespace

std::string piece_list_input(PieceListWorkload workload)
{
    if (replaces_all(workload))
    {
        return repeated_lines("foo bar baz qux\n", replace_all_lines);
    }
    return repeated_lines(
        "abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghi\n",
        document_lines);
}

std::expected<std::uint64_t, ProbeFailure> piece_list_run(PieceListWorkload workload,
                                                          const std::string &input,
                                                          application::TimingPort &timing)
{
    if (input != piece_list_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    switch (workload)
    {
    case PieceListWorkload::reads_after_1:
    case PieceListWorkload::reads_after_1000:
    case PieceListWorkload::reads_after_4000:
    case PieceListWorkload::reads_after_16000:
        return run_reads(workload, input, timing);
    case PieceListWorkload::replaces_after_1:
    case PieceListWorkload::replaces_after_1000:
    case PieceListWorkload::replaces_after_4000:
    case PieceListWorkload::replaces_after_16000:
        return run_replaces(workload, input, timing);
    case PieceListWorkload::replace_all_after_1:
    case PieceListWorkload::replace_all_after_8000:
        return run_replace_all(workload, input, timing);
    case PieceListWorkload::frame_after_1:
    case PieceListWorkload::frame_after_16000:
        return run_frame(workload, input, timing);
    }
    std::unreachable();
}
} // namespace nenenib::tests::performance
