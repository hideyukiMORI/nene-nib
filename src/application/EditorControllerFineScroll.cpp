#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "DisplayForm.hpp"
#include "DisplayInquiry.hpp"
#include "ScrollCarry.hpp"
#include "ScrollExtent.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "ScrollState.hpp"
#include "VisualPosition.hpp"
#include "VisualTextView.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <variant>

// ホイールとタッチパッドの端数（#1898・ADR 0009 の決定 6 の 2026-11-20 の追補）。端数は
// ScrollState::fraction の 1 欄で、置くのは finely_scrolled（送り）と、保つか 0 に戻すかを決める
// kept_fraction・align_scroll_before の 3 か所。行の単位で窓を決める道は ScrollState を作り直すので
// 0 に戻る。
namespace nenenib::application
{
namespace
{
// 端数を保つ意図。ホイール（本文・もう 1 つの枠・面とエクスプローラーとタブの帯）と本文のマウス
// （クリック・ドラッグ・右クリックのメニュー）と、使う人の操作ではない意図（合図・窓の寸法・
// hover）。鍵・打鍵・モードの切り替え・タブの切り替えなど、ほかの意図は写す前に行へ揃える。
[[nodiscard]] bool keeps_scroll_fraction(const EditorIntent &intent) noexcept
{
    return detail::not_user_operation(intent) || std::holds_alternative<ScrollLines>(intent) ||
           std::holds_alternative<ScrollPane>(intent) ||
           std::holds_alternative<ScrollTabs>(intent) ||
           std::holds_alternative<ScrollPaletteList>(intent) ||
           std::holds_alternative<ScrollExplorer>(intent) ||
           std::holds_alternative<ScrollCompletion>(intent) ||
           std::holds_alternative<ScrollSettings>(intent) ||
           std::holds_alternative<ScrollPreviewTable>(intent) ||
           std::holds_alternative<PointText>(intent) ||
           std::holds_alternative<PlaceCaret>(intent) ||
           std::holds_alternative<OpenContextMenu>(intent) ||
           std::holds_alternative<PointContextMenu>(intent) ||
           std::holds_alternative<StepContextMenu>(intent) ||
           std::holds_alternative<CloseContextMenu>(intent);
}
} // namespace

bool EditorController::keeps_fractions(const EditorState &state) noexcept
{
    return state.display() == core::DisplayForm::text && !display_pending(state.display_inquiry());
}

std::expected<std::size_t, TextLayoutFailure>
EditorController::kept_fraction(VisualTextView &view, core::VisualPosition at, std::size_t fraction,
                                core::ScrollExtent extent)
{
    if (fraction == 0)
    {
        return 0;
    }
    const auto next = view.reached(at, 1);
    if (!next)
    {
        return std::unexpected(next.error());
    }
    if (next.value().stopped)
    {
        return 0;
    }
    const auto bound = view.constrained(next.value().at, extent);
    if (!bound)
    {
        return std::unexpected(bound.error());
    }
    return bound.value() == at ? 0 : fraction;
}

std::expected<ScrollState, TextLayoutFailure>
EditorController::finely_scrolled(VisualTextView &view, const ScrollState &scroll,
                                  const ScrollLines &wheel, core::ScrollExtent extent)
{
    const auto carry = fine_carried(scroll.fraction, wheel.lines, wheel.parts);
    const auto moved =
        view.reached(core::VisualPosition{scroll.first_visible, scroll.first_row}, carry.lines);
    if (!moved)
    {
        return std::unexpected(moved.error());
    }
    const auto within = view.constrained(moved.value().at, extent);
    if (!within)
    {
        return std::unexpected(within.error());
    }
    // 本文の先頭か最後で止まった・上限で収めた送りは端数を持たない。
    const bool edge = moved.value().stopped || !(within.value() == moved.value().at);
    const auto fraction = kept_fraction(view, within.value(),
                                        edge ? 0 : static_cast<std::size_t>(carry.rest), extent);
    if (!fraction)
    {
        return std::unexpected(fraction.error());
    }
    return ScrollState{within.value().line, scroll.visible_lines, within.value().row,
                       scroll.horizontal, fraction.value()};
}

std::optional<std::int64_t> EditorController::coarse_wheel(std::int32_t lines, std::int32_t parts)
{
    const auto carry = coarse_carried(coarse_wheel_, lines, parts);
    coarse_wheel_ = carry.rest;
    if (carry.lines == 0)
    {
        return std::nullopt;
    }
    return carry.lines;
}

bool EditorController::scrolled_other_by_parts(const std::optional<EditorState> &pane,
                                               const ScrollPane &intent)
{
    const bool fine = state_.preview() == nullptr && pane.has_value() &&
                      keeps_fractions(pane.value()) &&
                      (intent.parts != 0 || pane.value().scroll().fraction != 0);
    if (!fine)
    {
        if (intent.parts == 0)
        {
            return false;
        }
        const auto lines = coarse_wheel(intent.lines, intent.parts);
        if (lines.has_value())
        {
            accept(ScrollPane{static_cast<std::int32_t>(lines.value())});
        }
        return true;
    }
    coarse_wheel_ = 0;
    const EditorState &state = pane.value();
    VisualTextView view(state, ports_.text_layouts, view_geometry_);
    const auto moved =
        finely_scrolled(view, state.scroll(), ScrollLines{intent.lines, intent.parts},
                        detail::scroll_extent_for(state_.mode()));
    if (!moved)
    {
        layout_failure_ = moved.error();
        return true;
    }
    state_ = state_.with_other_scrolled(moved.value().first_visible, moved.value().first_row,
                                        moved.value().fraction);
    return true;
}

void EditorController::scroll_preview_by_wheel(const ScrollLines &wheel)
{
    if (wheel.parts == 0)
    {
        scroll_preview(wheel.lines);
        return;
    }
    const auto lines = coarse_wheel(wheel.lines, wheel.parts);
    if (lines.has_value())
    {
        scroll_preview(lines.value());
    }
}

void EditorController::align_scroll_before(const EditorIntent &intent)
{
    if (state_.scroll().fraction == 0 || keeps_scroll_fraction(intent))
    {
        return;
    }
    auto scroll = state_.scroll();
    scroll.fraction = 0;
    state_ = state_.with_scroll(scroll);
}
} // namespace nenenib::application
