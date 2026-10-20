#include "EditorWindow.hpp"

#include "BodyLayout.hpp"
#include "PointText.hpp"
#include "ScrollLines.hpp"

#include <algorithm>
#include <windowsx.h>

namespace nenenib::ui::win32
{
void EditorWindow::point_text(POINT point, core::SelectionAnchoring anchoring)
{
    selection_point_ = point;
    const auto body = active_body(controller_.delivery().settings.font_size);
    const auto row =
        static_cast<std::int64_t>(std::max(point.y - body.content.top, 0L) / body.line_height);
    send(application::PointText{static_cast<float>(point.x - body.content.left), row, anchoring});
}

void EditorWindow::begin_selection_drag()
{
    if (mode_ != core::EditMode::ordinary || controller_.is_composing() || window_ == nullptr)
    {
        return;
    }
    SetCapture(window_);
    selecting_ = GetCapture() == window_;
    if (selecting_)
    {
        // ポインタが止まっていても画面外への選択を継続する。本文の編集は行わない。
        constexpr UINT interval_milliseconds = 50;
        static_cast<void>(SetTimer(window_, selection_timer, interval_milliseconds, nullptr));
    }
}

void EditorWindow::end_selection_drag()
{
    if (!selecting_)
    {
        return;
    }
    selecting_ = false;
    KillTimer(window_, selection_timer);
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
}

void EditorWindow::drag_selection(LPARAM data)
{
    if (selecting_)
    {
        point_text(POINT{GET_X_LPARAM(data), GET_Y_LPARAM(data)}, core::SelectionAnchoring::extend);
    }
}

void EditorWindow::scroll_selection()
{
    if (!selecting_)
    {
        return;
    }
    const auto point = selection_point_;
    // ドラッグの選択は操作中の枠だけ（ADR 0148 の決定 11）。
    const auto body = active_body(controller_.delivery().settings.font_size);
    const auto bottom =
        body.content.top + static_cast<std::int32_t>(body.visible_lines) * body.line_height;
    const std::int32_t direction = point.y < body.content.top ? -1 : (point.y >= bottom ? 1 : 0);
    if (direction != 0)
    {
        send(application::ScrollLines{direction});
    }
    if (direction != 0 || point.x < body.content.left || point.x >= body.content.right)
    {
        point_text(point, core::SelectionAnchoring::extend);
    }
}
} // namespace nenenib::ui::win32
