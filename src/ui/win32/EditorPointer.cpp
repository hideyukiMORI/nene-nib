#include "EditorWindow.hpp"

#include "BodyLayout.hpp"
#include "Direct2DRenderer.hpp"
#include "EditorWindowDetail.hpp"
#include "PointPreview.hpp"
#include "PointText.hpp"
#include "ScrollLines.hpp"
#include "SelectionAnchoring.hpp"
#include "TogglePreviewTask.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <windowsx.h>

namespace nenenib::ui::win32
{
void EditorWindow::point_text(POINT point, core::SelectionAnchoring anchoring)
{
    selection_point_ = point;
    const auto body = active_body(controller_.delivery().settings);
    const auto row =
        static_cast<std::int64_t>(std::max(point.y - body.content.top, 0L) / body.line_height);
    send(application::PointText{static_cast<float>(point.x - body.content.left), row, anchoring});
}

// プレビューの枠は移る先ではないので、押しても本文のキャレットは動かさない（ADR 0155）。押した点の
// 下のチェックボックスだけを覚える（ADR 0155 の追補 24）。
void EditorWindow::press_preview_task(LPARAM data)
{
    preview_task_press_.press(
        renderer_ == nullptr ? std::nullopt
                             : renderer_->preview_task_at(GET_X_LPARAM(data), GET_Y_LPARAM(data)));
}

void EditorWindow::release_preview_task(LPARAM data)
{
    if (!preview_task_press_.pressed())
    {
        return;
    }
    // 面（Ctrl+P）が開いている間は帯の離すと同じく送らない。押しは release で必ず消える。
    const bool reachable =
        renderer_ != nullptr && previewing() && !controller_.command_palette_active();
    const auto block = preview_task_press_.release(
        reachable ? renderer_->preview_task_at(GET_X_LPARAM(data), GET_Y_LPARAM(data))
                  : std::nullopt);
    if (block.has_value())
    {
        send(application::TogglePreviewTask{block.value()});
    }
}

void EditorWindow::press_preview_text(LPARAM data)
{
    const auto point = renderer_ == nullptr
                           ? std::nullopt
                           : renderer_->preview_point_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
    if (!point.has_value() || window_ == nullptr)
    {
        return;
    }
    send(application::PointPreview{point.value(), detail::held(VK_SHIFT)
                                                      ? core::SelectionAnchoring::extend
                                                      : core::SelectionAnchoring::collapse});
    SetCapture(window_);
    preview_dragging_ = GetCapture() == window_;
}

void EditorWindow::end_preview_drag()
{
    if (!preview_dragging_)
    {
        return;
    }
    // 捕まえを放すと WM_CAPTURECHANGED がここへ戻るので、先に印を消す。
    preview_dragging_ = false;
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
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
    end_preview_drag();
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
    // 全面プレビューの選択は描いた layout の文字で伸ばす（改訂 18・面が閉じていれば止める）。
    if (preview_dragging_)
    {
        const auto point = renderer_ != nullptr && full_previewing()
                               ? renderer_->preview_point_at(GET_X_LPARAM(data), GET_Y_LPARAM(data))
                               : std::nullopt;
        if (point.has_value())
        {
            send(application::PointPreview{point.value(), core::SelectionAnchoring::extend});
        }
        return;
    }
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
    const auto body = active_body(controller_.delivery().settings);
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
