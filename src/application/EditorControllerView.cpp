#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "DisplayInquiry.hpp"
#include "ScreenMotion.hpp"
#include "TextCaret.hpp"
#include "TextPosition.hpp"
#include "VisualPosition.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <utility>

namespace nenenib::application
{
using detail::scroll_extent_for;

namespace
{
[[nodiscard]] core::ScrollFollow scroll_follow_for(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::ScrollFollow::minimal;
    case core::EditMode::vim:
        return core::ScrollFollow::vim;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<ScreenMotion> screen_motion(core::CaretMotion motion,
                                                        std::size_t height)
{
    switch (motion)
    {
    case core::CaretMotion::previous_line:
        return ScreenMotion{-1, std::nullopt};
    case core::CaretMotion::next_line:
        return ScreenMotion{1, std::nullopt};
    case core::CaretMotion::page_up:
        return ScreenMotion{-static_cast<std::int64_t>(height), std::nullopt};
    case core::CaretMotion::page_down:
        return ScreenMotion{static_cast<std::int64_t>(height), std::nullopt};
    case core::CaretMotion::line_start:
        // 行の原点より左の最小の x。巨大な行の窓の x は窓の始まりからなので、0 では行頭に
        // 着かない（ADR 0139 の決定 4）。
        return ScreenMotion{0, std::numeric_limits<float>::lowest()};
    case core::CaretMotion::line_end:
        return ScreenMotion{0, std::numeric_limits<float>::max()};
    case core::CaretMotion::previous_character:
    case core::CaretMotion::next_character:
    case core::CaretMotion::previous_word:
    case core::CaretMotion::next_word:
    case core::CaretMotion::document_start:
    case core::CaretMotion::document_end:
        return std::nullopt;
    }
    std::unreachable();
}

[[nodiscard]] bool page_motion(core::CaretMotion motion) noexcept
{
    return motion == core::CaretMotion::page_up || motion == core::CaretMotion::page_down;
}
} // namespace

void EditorController::follow_caret()
{
    follow_position(state_.text().position_of(state_.selection().caret));
}

// 位置が見える範囲に入るようにスクロールすることを頼む。キャレットと incsearch の preview が
// 同じ計算を使う（ADR 0041 の決定 3）。layout は作らず、解決はスクロールを読む所で 1 回だけ
// （ADR 0128 の決定 1・2）。
void EditorController::follow_position(core::TextPosition position)
{
    pending_follow_ = position;
}

// 保留を先に空にしてから view を作るので、visual_view() の先頭の解決へ再入しない。解決はその
// 時点の state で 1 回（ADR 0128 の決定 4）。保留の clamp は追従より先（決定 8）。問いの答えを
// 待つ間は文字組みせず、保留の追従と clamp を消費せずに残して戻る。答えて表示形式が決まった後の
// 最初のスクロールの読みで解決する（ADR 0140 の決定 2）。
void EditorController::settle_follow()
{
    if (display_pending(state_.display_inquiry()))
    {
        return;
    }
    const bool clamp = std::exchange(pending_clamp_, false);
    const auto pending = std::exchange(pending_follow_, std::nullopt);
    if (clamp)
    {
        clamp_scroll();
    }
    if (!pending.has_value())
    {
        return;
    }
    const auto followed =
        visual_view().followed(core::TextCaret{pending.value(), state_.caret_affinity()},
                               scroll_follow_for(state_.mode()), scroll_extent_for(state_.mode()));
    if (!followed)
    {
        layout_failure_ = followed.error();
        return;
    }
    if (!(followed.value() == state_.scroll()))
    {
        state_ = state_.with_scroll(followed.value());
    }
}

// 問いの答えを待つ間の保留は、追従も clamp も、その文書を離れるときに捨てる。問いの答え
// （SelectDisplay）が clamp_scroll と follow_caret を頼み直すので失うものは無い（ADR 0128 の
// 決定 7）。
void EditorController::drop_pending_follow()
{
    if (display_pending(state_.display_inquiry()))
    {
        pending_follow_.reset();
        pending_clamp_ = false;
    }
}

const ScrollState &EditorController::settled_scroll()
{
    settle_follow();
    return state_.scroll();
}

void EditorController::clamp_scroll()
{
    // 起動時は幅が未確定。保存した画面行を最初のVisibleLinesまで保つ。問いの答えを待つ間も保つ
    // （ADR 0140 の決定 2・答えの SelectDisplay が収め直す）。
    if ((view_geometry_.width <= 0 && settled_scroll().first_row > 0) ||
        display_pending(state_.display_inquiry()))
    {
        return;
    }
    const auto &scroll = settled_scroll();
    const auto within =
        visual_view().constrained(core::VisualPosition{scroll.first_visible, scroll.first_row},
                                  scroll_extent_for(state_.mode()));
    if (!within)
    {
        layout_failure_ = within.error();
        return;
    }
    state_ = state_.with_scroll(ScrollState{within.value().line, scroll.visible_lines,
                                            within.value().row, scroll.horizontal});
}

// view はスクロールを読むので、頼まれた追従を先に解決する（ADR 0128 の決定 2）。
// 問いの答えを待つ間は幅の無い窓として解決し、文字組みの port を呼ばない（ADR 0140 の決定 2）。
VisualTextView EditorController::visual_view()
{
    settle_follow();
    return VisualTextView{state_, ports_.text_layouts,
                          display_pending(state_.display_inquiry()) ? core::TextViewGeometry{}
                                                                    : view_geometry_};
}

bool EditorController::move_visual(const MoveCaret &intent)
{
    const auto motion = screen_motion(intent.motion, settled_scroll().visible_lines);
    if (view_geometry_.width <= 0 || state_.mode() != core::EditMode::ordinary ||
        !motion.has_value())
    {
        wanted_x_.reset();
        return false;
    }
    auto view = visual_view();
    const auto source = state_.text().position_of(state_.selection().caret);
    const auto point = view.location(core::TextCaret{source, state_.caret_affinity()});
    if (!point)
    {
        layout_failure_ = point.error();
        return true;
    }
    const float x = motion.value().x.value_or(wanted_x_.value_or(point.value().x));
    wanted_x_ = motion.value().x.has_value() ? std::nullopt : std::optional{x};
    const auto moved =
        view.shifted(core::VisualPosition{source.line, point.value().row}, motion.value().rows);
    if (!moved)
    {
        layout_failure_ = moved.error();
        return true;
    }
    const auto caret = view.caret_at(moved.value(), x);
    if (!caret)
    {
        layout_failure_ = caret.error();
        return true;
    }
    // PgDn / PgUp は画面の先頭も同じ数の画面行だけ送る（ADR 0144 の決定 7）。端は本文の最初と最後の
    // 画面行で止まり、追従（画面を埋める）がキャレットの見える範囲へ収める。
    if (page_motion(intent.motion))
    {
        const auto &scroll = state_.scroll();
        const auto top = view.shifted(core::VisualPosition{scroll.first_visible, scroll.first_row},
                                      motion.value().rows);
        if (!top)
        {
            layout_failure_ = top.error();
            return true;
        }
        state_ = state_.with_scroll(ScrollState{top.value().line, scroll.visible_lines,
                                                top.value().row, scroll.horizontal});
    }
    move_caret_to(state_.text().offset_of(caret.value().position), intent.anchoring,
                  caret.value().affinity);
    return true;
}

// 見えている範囲は先頭の (行, 画面行) から見えている行数ぶんの画面行。キャレットが外れていれば、
// 外れた側の端の画面行へ今の横の位置のまま入れる（#785）。位置の解決は上下の移動（move_visual）と
// 同じ location・shifted・caret_at で、格子と折り返しは view が答える。
std::expected<core::TextCaret, TextLayoutFailure> EditorController::caret_in_view()
{
    const ScrollState scroll = settled_scroll();
    auto view = visual_view();
    const core::TextCaret caret{state_.text().position_of(state_.selection().caret),
                                state_.caret_affinity()};
    const auto point = view.location(caret);
    if (!point)
    {
        return std::unexpected(point.error());
    }
    const core::VisualPosition at{caret.position.line, point.value().row};
    const core::VisualPosition first{scroll.first_visible, scroll.first_row};
    const auto rows = std::max<std::size_t>(scroll.visible_lines, 1) - 1;
    const auto last = view.shifted(first, static_cast<std::int64_t>(rows));
    if (!last)
    {
        return std::unexpected(last.error());
    }
    if (!(at < first) && !(last.value() < at))
    {
        return caret;
    }
    return view.caret_at(at < first ? first : last.value(), point.value().x);
}

void EditorController::accept(const PointText &intent)
{
    if (state_.composition().has_value())
    {
        return;
    }
    const auto &scroll = settled_scroll();
    const auto row = std::clamp<std::int64_t>(intent.row, 0,
                                              static_cast<std::int64_t>(scroll.visible_lines) - 1);
    auto view = visual_view();
    const auto at = view.shifted(core::VisualPosition{scroll.first_visible, scroll.first_row}, row);
    if (!at)
    {
        layout_failure_ = at.error();
        return;
    }
    const auto caret = view.caret_at(at.value(), intent.x + scroll.horizontal);
    if (!caret)
    {
        layout_failure_ = caret.error();
        return;
    }
    accept(PlaceCaret{caret.value().position, intent.anchoring, caret.value().affinity});
}
} // namespace nenenib::application
