#include "FrameProbe.hpp"

#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "GiantLine.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SelectEditMode.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimSearchPattern.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <string>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
// 1 行の長さは巨大な行のしきい値ちょうど（超えると格子表示を聞く別の frame になる・ADR 0140）。
// ASCII の 3 本と sparse は表示文字列がこの長さ、混合は表示の 7
// コードポイントの単位をしきい値まで。
constexpr std::size_t line_length = core::giant_line_threshold;
constexpr std::size_t ascii_units = line_length / 4U;
constexpr std::size_t mixed_display_unit = 7U;
constexpr std::size_t mixed_source_unit = 6U;
constexpr std::size_t mixed_units = line_length / mixed_display_unit;
// 選択の始まりは 4 分の 1、終わりは 4 分の 3 の位置の a（1 始まりの原文桁）。
constexpr std::size_t quarter = ascii_units + 1U;
constexpr std::size_t three_quarters = 3U * ascii_units + 1U;
static_assert(line_length % 16U == 0U, "the quarter positions must land on an 'a'");
static_assert(mixed_units * mixed_display_unit <= line_length, "the mixed line is not giant");

[[nodiscard]] std::size_t frame_index(ProbeWorkload workload)
{
    return static_cast<std::size_t>(workload) -
           static_cast<std::size_t>(ProbeWorkload::frame_dense_ascii);
}

[[nodiscard]] std::string repeated_frame_text(std::string_view unit, std::size_t count)
{
    std::string text;
    text.reserve(unit.size() * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        text += unit;
    }
    return text;
}

void place_frame_caret(application::EditorController &controller, std::size_t column,
                       core::SelectionAnchoring anchoring)
{
    static_cast<void>(controller.apply(
        application::PlaceCaret{{core::LineNumber{1U}, core::Column{column}}, anchoring}));
}

// VISUAL の「v{半分}l」。1 始まりの quarter から 2 * ascii_units 文字ぶん右へ広げる。
void extend_visual(application::EditorController &controller)
{
    std::u32string keys = U"v";
    for (const char digit : std::to_string(2U * ascii_units))
    {
        keys += static_cast<char32_t>(digit);
    }
    keys += U'l';
    for (const char32_t key : keys)
    {
        static_cast<void>(controller.apply(application::VimKeyPress{core::VimCharacter{key}}));
    }
}

void prepare_frame(Editing &editing, std::size_t index, const std::string &input)
{
    auto &controller = editing.controller();
    editing.files().hold(input);
    static_cast<void>(controller.apply(application::VisibleLines{1U}));
    static_cast<void>(controller.apply(
        application::OpenDocument{core::FilePath::parse("C:\\nib-probe\\frame.txt").value()}));
    if (index == 3U)
    {
        static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
        place_frame_caret(controller, quarter, core::SelectionAnchoring::collapse);
        place_frame_caret(controller, three_quarters, core::SelectionAnchoring::extend);
        return;
    }
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(controller.apply(application::VimKeyPress{
        core::VimSearchPattern{"a", core::VimSearchDirection::forward, std::nullopt}}));
    const std::array<std::size_t, 5> columns{1U, 2U, line_length, three_quarters, quarter};
    place_frame_caret(controller, columns.at(index), core::SelectionAnchoring::collapse);
    if (index == 4U)
    {
        extend_visual(controller);
    }
}

[[nodiscard]] bool frame_state_equals(const application::EditorFrame &frame, std::size_t index)
{
    const std::array<std::size_t, 5> columns{1U, 2U, line_length, three_quarters, three_quarters};
    const core::EditMode mode = index == 3U ? core::EditMode::ordinary : core::EditMode::vim;
    const core::VimMode vim = index == 4U ? core::VimMode::visual : core::VimMode::normal;
    return frame.lines.size() == 1U && frame.first_visible.value == 1U && frame.total_lines == 1U &&
           frame.caret.position.line.value == 1U &&
           frame.caret.position.column.value == columns.at(index) && frame.mode == mode &&
           frame.vim_mode == vim && !frame.command_line.has_value() &&
           !frame.composition.has_value() && !frame.command_composition.has_value() &&
           !frame.document.last_failure.has_value();
}

// 混合の 1 単位は原文 6 コードポイント（日 a Tab 🖋 ^A 空白）で、表示は ^A が 2 つなので 7。
[[nodiscard]] std::size_t expected_display_start(std::size_t index, std::size_t at)
{
    if (index != 1U)
    {
        return at;
    }
    constexpr std::array<std::size_t, mixed_source_unit> mixed{0U, 1U, 2U, 3U, 4U, 6U};
    return at == mixed_source_unit * mixed_units
               ? mixed_display_unit * mixed_units
               : mixed_display_unit * (at / mixed_source_unit) + mixed.at(at % mixed_source_unit);
}

[[nodiscard]] bool display_equals(const application::LineView &line, std::size_t index,
                                  const std::string &input)
{
    const std::string expected =
        index == 1U ? repeated_frame_text("日a\t🖋^A ", mixed_units) : input;
    const std::size_t size = index == 1U ? mixed_source_unit * mixed_units + 1U : line_length + 1U;
    if (line.number.value != 1U || line.text != input || line.display.text != expected ||
        line.display.starts.size() != size)
    {
        return false;
    }
    for (std::size_t at = 0; at < size; ++at)
    {
        if (line.display.starts.at(at) != expected_display_start(index, at))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool matches_equal(const application::LineView &line, std::size_t index)
{
    const std::array<std::size_t, 5> counts{ascii_units, mixed_units, 1U, 0U, ascii_units};
    const std::array<std::size_t, 5> strides{4U, mixed_source_unit, 0U, 0U, 4U};
    const std::array<std::size_t, 5> bases{1U, 2U, line_length, 0U, 1U};
    if (line.matches.size() != counts.at(index))
    {
        return false;
    }
    for (std::size_t at = 0; at < counts.at(index); ++at)
    {
        const std::size_t begin = bases.at(index) + strides.at(index) * at;
        if (line.matches.at(at) != core::SelectionSpan{core::SelectionPresence::present,
                                                       core::Column{begin},
                                                       core::Column{begin + 1U}})
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] core::SelectionSpan expected_frame_selection(std::size_t index)
{
    if (index < 3U)
    {
        return core::no_selection_span();
    }
    return core::SelectionSpan{core::SelectionPresence::present, core::Column{quarter},
                               core::Column{index == 3U ? three_quarters : three_quarters + 1U}};
}

[[nodiscard]] bool current_match_equals(const application::LineView &line, std::size_t index)
{
    if (index == 3U)
    {
        return !line.current_match.has_value();
    }
    if (!line.current_match.has_value())
    {
        return false;
    }
    const std::array<std::size_t, 5> begins{1U, 2U, line_length, 0U, three_quarters};
    const std::size_t begin = begins.at(index);
    return line.current_match.value() == core::SelectionSpan{core::SelectionPresence::present,
                                                             core::Column{begin},
                                                             core::Column{begin + 1U}};
}

[[nodiscard]] std::uint64_t frame_checksum(const std::string &input,
                                           const application::EditorFrame &frame)
{
    const auto &line = frame.lines.at(0);
    std::uint64_t checksum = checksum_of(input) + checksum_of(line.display.text) +
                             line.number.value + frame.first_visible.value + frame.total_lines +
                             frame.lines.size() + frame.caret.position.line.value +
                             frame.caret.position.column.value + line.matches.size();
    for (const auto start : line.display.starts)
    {
        checksum += start;
    }
    for (const auto &match : line.matches)
    {
        checksum += match.begin.value + match.end.value;
    }
    if (line.selection.presence == core::SelectionPresence::present)
    {
        checksum += 1U + line.selection.begin.value + line.selection.end.value;
    }
    if (line.current_match.has_value())
    {
        checksum +=
            1U + line.current_match.value().begin.value + line.current_match.value().end.value;
    }
    return checksum;
}
} // namespace

std::string frame_probe_input(ProbeWorkload workload)
{
    const std::size_t index = frame_index(workload);
    if (index == 1U)
    {
        return repeated_frame_text("日a\t🖋\x01 ", mixed_units);
    }
    if (index == 2U)
    {
        return std::string(line_length - 1U, 'x') + "a";
    }
    return repeated_frame_text("a x ", ascii_units);
}

std::expected<std::uint64_t, ProbeFailure>
framed_line(ProbeWorkload workload, const std::string &input, application::TimingPort &timing)
{
    if (input != frame_probe_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const std::size_t index = frame_index(workload);
    Editing editing;
    prepare_frame(editing, index, input);
    timing.mark(core::Milestone::probe_started);
    const auto frame = editing.controller().frame();
    timing.mark(core::Milestone::probe_finished);
    if (!frame_state_equals(frame, index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &line = frame.lines.at(0);
    if (!display_equals(line, index, input) || !matches_equal(line, index) ||
        line.selection != expected_frame_selection(index) || !current_match_equals(line, index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return frame_checksum(input, frame);
}
} // namespace nenenib::tests::performance
