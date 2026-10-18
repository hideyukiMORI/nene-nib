#include "EditorController.hpp"

#include "DisplayInquiry.hpp"
#include "ScreenMotion.hpp"

#include <algorithm>
#include <limits>

namespace nenenib::application
{
namespace
{
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

// engine が求めた行の画面行数を、行の順の列へ足す。同じ行を 2 度求めたら往復が止まらないので失敗。
[[nodiscard]] std::expected<void, TextLayoutFailure>
measured_rows(std::vector<core::VimLineRows> &rows, core::LineNumber line, VisualTextView &text)
{
    const auto found = std::ranges::lower_bound(
        rows, line.value, {}, [](const core::VimLineRows &value) { return value.line.value; });
    if (found != rows.end() && found->line == line)
    {
        return std::unexpected(TextLayoutFailure::unavailable);
    }
    const auto count = text.rows(line);
    if (!count)
    {
        return std::unexpected(count.error());
    }
    rows.insert(found, core::VimLineRows{line, count.value()});
    return {};
}

// engine が求めた行の中の画面行を、答えの列へ足す（ADR 0144 の決定 2）。同じ答えを 2 度足したら
// 往復が止まらないので失敗。
[[nodiscard]] std::expected<void, TextLayoutFailure>
measured_span(std::vector<core::VimRowSpan> &spans, const core::VimRowRequest &request,
              VisualTextView &text)
{
    const auto span = text.row_span(request);
    if (!span)
    {
        return std::unexpected(span.error());
    }
    const auto known = std::ranges::any_of(
        spans, [&span](const core::VimRowSpan &value)
        { return value.line == span.value().line && value.row == span.value().row; });
    if (known)
    {
        return std::unexpected(TextLayoutFailure::unavailable);
    }
    spans.push_back(span.value());
    return {};
}

[[nodiscard]] bool page_motion(core::CaretMotion motion) noexcept
{
    return motion == core::CaretMotion::page_up || motion == core::CaretMotion::page_down;
}
} // namespace

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

std::expected<core::VimStep, TextLayoutFailure>
EditorController::measured_vim_step(const core::VimKey &key)
{
    std::vector<core::VimLineRows> rows;
    std::vector<core::VimRowSpan> spans;
    // 表示領域は engine が求めたときだけ渡す（ADR 0129 の決定 2）。追従を解決するのはその時だけ。
    core::VimEditorView view{
        state_.text(), state_.selection(), std::nullopt,
        replay_depth_.has_value() ? core::VimKeySource::replayed : core::VimKeySource::typed,
        core::VimTabs{state_.active_tab(), state_.tab_count()},
        // 格子は常に折り返す（ADR 0140 の決定 7）。wrap off の text 形は無効のまま（gj gk は j k・
        // ADR 0145 の決定 4）。横送りの row_request は有効か否かに関わらず答える。
        // 格子の画面移動は画面行の単位（ADR 0144 の決定 5）。text の形は wrap でも論理行のまま。
        // 1 画面行の升の数は文字組みの問いに答えるときに一緒に置く（決定 10）。
        core::VimLayoutView{view_geometry_.width > 0 &&
                                (state_.settings().wrapping == core::LineWrapping::wrap ||
                                 state_.display() == core::DisplayForm::grid),
                            rows,
                            state_.display() == core::DisplayForm::grid ? core::VimScrollUnit::row
                                                                        : core::VimScrollUnit::line,
                            spans}};
    auto step = core::vim_step(state_.vim(), view, key);
    // どの要求も同じ鍵をもう一度流す。viewport は 1 度渡したら以後の往復でも渡したまま。
    while (step.viewport_request || step.layout_request.has_value() || step.row_request.has_value())
    {
        const auto answered = answered_vim_request(step, view, rows, spans);
        if (!answered)
        {
            return std::unexpected(answered.error());
        }
        step = core::vim_step(state_.vim(), view, key);
    }
    return step;
}

std::expected<void, TextLayoutFailure>
EditorController::answered_vim_request(const core::VimStep &step, core::VimEditorView &view,
                                       std::vector<core::VimLineRows> &rows,
                                       std::vector<core::VimRowSpan> &spans)
{
    if (step.layout_request.has_value() || step.row_request.has_value())
    {
        // 1 画面行の升の数は文字組みの問いに答える所で 1 回だけ置く（ADR 0144 の決定 10）。
        auto text = visual_view();
        view.layout.row_columns = text.row_columns();
        if (step.layout_request.has_value())
        {
            const auto measured = measured_rows(rows, step.layout_request.value(), text);
            view.layout.lines = rows;
            return measured;
        }
        const auto measured = measured_span(spans, step.row_request.value(), text);
        view.layout.spans = spans;
        return measured;
    }
    if (view.viewport.has_value())
    {
        return std::unexpected(TextLayoutFailure::unavailable);
    }
    const auto scroll = settled_scroll();
    // 画面の幅（升）と今の横位置（leftcol）も同じ往復で答える（ADR 0145 の決定 2）。横に送らない
    // 文書では幅 0 で、文字組みを呼ばない。
    auto text = visual_view();
    const auto columns = text.horizontal_columns();
    const auto left =
        columns == 0
            ? std::expected<std::size_t, TextLayoutFailure>{0}
            : text.column_of_horizontal(state_.text().position_of(state_.selection().caret).line,
                                        scroll.horizontal);
    if (!left)
    {
        return std::unexpected(left.error());
    }
    view.viewport = core::VimViewport{scroll.first_visible, scroll.visible_lines, scroll.first_row,
                                      columns, left.value()};
    return {};
}
} // namespace nenenib::application
