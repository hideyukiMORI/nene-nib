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
} // namespace

// 問いの答えを待つ間は幅の無い窓として解決し、文字組みの port を呼ばない（ADR 0140 の決定 2）。
VisualTextView EditorController::visual_view() const
{
    return VisualTextView{state_, ports_.text_layouts,
                          display_pending(state_.display_inquiry()) ? core::TextViewGeometry{}
                                                                    : view_geometry_};
}

bool EditorController::move_visual(const MoveCaret &intent)
{
    const auto motion = screen_motion(intent.motion, state_.scroll().visible_lines);
    if (view_geometry_.width <= 0 || state_.mode() != core::EditMode::ordinary ||
        !motion.has_value())
    {
        wanted_x_.reset();
        return false;
    }
    const auto view = visual_view();
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
    const auto &scroll = state_.scroll();
    const auto row = std::clamp<std::int64_t>(intent.row, 0,
                                              static_cast<std::int64_t>(scroll.visible_lines) - 1);
    const auto view = visual_view();
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
EditorController::measured_vim_step(const core::VimKey &key) const
{
    const auto &scroll = state_.scroll();
    std::vector<core::VimLineRows> rows;
    core::VimEditorView view{
        state_.text(), state_.selection(),
        core::VimViewport{scroll.first_visible, scroll.visible_lines},
        replay_depth_.has_value() ? core::VimKeySource::replayed : core::VimKeySource::typed,
        core::VimTabs{state_.active_tab(), state_.tab_count()},
        // 格子は常に折り返す（ADR 0140 の決定 7）。
        core::VimLayoutView{view_geometry_.width > 0 &&
                                (state_.settings().wrapping == core::LineWrapping::wrap ||
                                 state_.display() == core::DisplayForm::grid),
                            scroll.first_row, rows}};
    auto step = core::vim_step(state_.vim(), view, key);
    const auto text = visual_view();
    while (step.layout_request.has_value())
    {
        const auto line = step.layout_request.value();
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
        view.layout.lines = rows;
        step = core::vim_step(state_.vim(), view, key);
    }
    return step;
}
} // namespace nenenib::application
