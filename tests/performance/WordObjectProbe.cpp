#include "WordObjectProbe.hpp"

#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "GiantLine.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "Milestone.hpp"
#include "PlaceCaret.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "ProbeWorkload.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimMode.hpp"
#include "VimRegisterKind.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRange.hpp"
#include "VimTextObjectRequest.hpp"
#include "VimTextObjectScope.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests::performance
{
namespace
{
// 期待の範囲は Vim 9.1 で同じ本文に `y{オブジェクト}` を打って測った値（#1424・`'[` と無名
// レジスタの長さ・キャレットは行の真ん中の 2 バイト後ろ）。区間で引く回数はどれも 1 回。
constexpr std::size_t giant_bytes = std::size_t{16} * 1024U * 1024U;
constexpr std::size_t shapes = 6U;
constexpr std::size_t caret_shift = 2U;
constexpr std::string_view solid_unit = "abcdefgh";
constexpr std::string_view spaced_unit = "lorem ipsum dolor sit amet ";
constexpr std::array<core::VimTextObjectScope, shapes> scopes{
    core::VimTextObjectScope::inner, core::VimTextObjectScope::around,
    core::VimTextObjectScope::inner, core::VimTextObjectScope::around,
    core::VimTextObjectScope::inner, core::VimTextObjectScope::inner};
constexpr std::array<core::VimTextObject, shapes> objects{
    core::VimTextObject::word, core::VimTextObject::word,         core::VimTextObject::word,
    core::VimTextObject::word, core::VimTextObject::double_quote, core::VimTextObject::paren};
// 期待の [begin, end)。引用符と括弧と solid の end は行の終わりから数える（from_line_end の行・
// 0 は行の終わり、1 は閉じの引用符・括弧の位置）。
constexpr std::array<std::size_t, 2U * shapes> begins{0U, 0U, 8193U,    8193U,    1U, 1U,
                                                      0U, 0U, 8388609U, 8388609U, 1U, 1U};
constexpr std::array<std::size_t, 2U * shapes> ends{0U, 0U, 8198U,    8199U,    1U, 1U,
                                                    0U, 0U, 8388614U, 8388615U, 1U, 1U};
constexpr std::array<bool, 2U * shapes> from_line_end{true, true, false, false, true, true,
                                                      true, true, false, false, true, true};

[[nodiscard]] std::size_t line_bytes(std::size_t index)
{
    return index < shapes ? core::giant_line_threshold : giant_bytes;
}

[[nodiscard]] std::string cycled(std::string_view unit, std::size_t length)
{
    std::string text;
    text.reserve(length + unit.size());
    while (text.size() < length)
    {
        text += unit;
    }
    text.resize(length);
    return text;
}

[[nodiscard]] std::string enclosed(char open, char close, std::size_t length)
{
    std::string text(1U, open);
    text += cycled(spaced_unit, length - 2U);
    text += close;
    return text;
}

[[nodiscard]] std::string shaped_line(std::size_t index)
{
    const std::size_t length = line_bytes(index);
    const std::size_t shape = index % shapes;
    if (shape < 2U)
    {
        return cycled(solid_unit, length);
    }
    if (shape < 4U)
    {
        return cycled(spaced_unit, length);
    }
    return shape == 4U ? enclosed('"', '"', length) : enclosed('(', ')', length);
}

[[nodiscard]] std::size_t expected_end(std::size_t index)
{
    return from_line_end.at(index) ? line_bytes(index) - ends.at(index) : ends.at(index);
}

[[nodiscard]] bool outcome_equals(const core::VimTextObjectOutcome &outcome, std::size_t index)
{
    const auto *span = std::get_if<core::VimTextObjectSpan>(&outcome);
    return span != nullptr && span->range.kind == core::VimRegisterKind::characters &&
           span->range.range.begin.value == begins.at(index) &&
           span->range.range.end.value == expected_end(index);
}

[[nodiscard]] std::size_t caret_of(std::size_t index)
{
    return (line_bytes(index) / 2U) + caret_shift;
}
} // namespace

std::string word_object_input(WordObjectWorkload workload)
{
    switch (workload)
    {
    case WordObjectWorkload::iw_solid_16384:
    case WordObjectWorkload::aw_solid_16384:
    case WordObjectWorkload::iw_spaced_16384:
    case WordObjectWorkload::aw_spaced_16384:
    case WordObjectWorkload::iquote_16384:
    case WordObjectWorkload::iparen_16384:
    case WordObjectWorkload::iw_solid_16mib:
    case WordObjectWorkload::aw_solid_16mib:
    case WordObjectWorkload::iw_spaced_16mib:
    case WordObjectWorkload::aw_spaced_16mib:
    case WordObjectWorkload::iquote_16mib:
    case WordObjectWorkload::iparen_16mib:
        return shaped_line(static_cast<std::size_t>(workload));
    case WordObjectWorkload::double_click_solid_16384:
        return cycled(solid_unit, core::giant_line_threshold);
    }
    std::unreachable();
}

std::expected<std::uint64_t, ProbeFailure> word_object_ranges(WordObjectWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto text = core::TextBuffer::from_utf8(input);
    const auto index = static_cast<std::size_t>(workload);
    if (!(index < 2U * shapes) || !text.has_value() || input != word_object_input(workload) ||
        text.value().line_count() != 1U || text.value().piece_count() != 1U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const core::Selection selection = core::collapsed_at(core::Offset{caret_of(index)});
    const core::VimTextObjectRequest request{scopes.at(index % shapes), objects.at(index % shapes),
                                             core::VimMode::normal};
    std::cerr << "preparedPieces=1 preparedHash=" << checksum_of(input) << '\n';
    timing.mark(core::Milestone::probe_started);
    const core::VimTextObjectOutcome outcome =
        core::vim_text_object_range(text.value(), selection, request, 1U);
    timing.mark(core::Milestone::probe_finished);
    if (!outcome_equals(outcome, index))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &span = std::get<core::VimTextObjectSpan>(outcome);
    return checksum_of(input) + 1U + span.range.range.begin.value + span.range.range.end.value;
}

// 通常モードで行の真ん中へキャレットを置いて画面を送ってから（区間の外）、見えている 3 行目を
// ダブルクリックする（区間）。空白の無い 1 語の行なので、どこを押しても選択は行の全体
// （Vim の `iw` と同じ・#1250）。選択は区間の外で Ctrl+C と同じ写しで読む。
std::expected<std::uint64_t, ProbeFailure> word_double_clicked(WordObjectWorkload workload,
                                                               const std::string &input,
                                                               application::TimingPort &timing)
{
    if (input != word_object_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(application::SelectEditMode{core::EditMode::ordinary}));
    static_cast<void>(controller.apply(application::InsertText{input}));
    static_cast<void>(
        controller.apply(application::VisibleLines{5U, core::TextViewGeometry{400, 20, 96}}));
    static_cast<void>(controller.apply(application::PlaceCaret{
        core::TextPosition{core::LineNumber{1U}, core::Column{(input.size() / 2U) + 1U}},
        core::SelectionAnchoring::collapse}));
    std::cerr << "preparedPieces=1 preparedHash=" << checksum_of(input) << '\n';
    timing.mark(core::Milestone::probe_started);
    static_cast<void>(controller.apply(application::PointText{
        55.0F, 2, core::SelectionAnchoring::collapse, application::PointerClicks::double_}));
    timing.mark(core::Milestone::probe_finished);
    static_cast<void>(
        controller.apply(application::ClipboardAction{application::ClipboardOperation::copy}));
    const std::string copied = editing.clipboard().read().value_or("");
    if (copied != input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) + copied.size();
}
} // namespace nenenib::tests::performance
