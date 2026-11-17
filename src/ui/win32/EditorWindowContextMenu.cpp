// 本文の右クリックのメニューの窓の側（ADR 0078 の追補 #1051・D71・FR-031）。右クリック・
// Shift+F10・メニューキーで開き、開いている間の鍵とマウスはメニューへ送る。何を出すか・何で閉じる
// か・選んだ行で何をするかは controller が決め、窓は意図を送るだけ（ARC-011）。当たりは renderer が
// 最後に描いた配置（core::context_menu_layout の 1 本）を引く。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ChooseContextMenu.hpp"
#include "CloseContextMenu.hpp"
#include "ContextMenuKey.hpp"
#include "LayoutRect.hpp"
#include "OpenContextMenu.hpp"
#include "PaneLayouts.hpp"
#include "PointContextMenu.hpp"
#include "PointerClicks.hpp"
#include "SelectionAnchoring.hpp"
#include "StepContextMenu.hpp"
#include "SwitchPane.hpp"

#include <windows.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
using detail::held;
using detail::high_word_of;
using detail::low_word_of;

namespace
{
[[nodiscard]] POINT point_of(LPARAM data) noexcept
{
    return POINT{low_word_of(data), high_word_of(data)};
}
} // namespace

// 操作中の枠の本文の中か。面・入力行・エクスプローラーの焦点の間は本文の右クリックにしない。
bool EditorWindow::in_text_body(POINT point) const
{
    if (controller_.command_palette_active() || controller_.command_line_active() ||
        controller_.explorer_focused())
    {
        return false;
    }
    const auto body = active_body(controller_.delivery().settings);
    return core::contains(body.content, point.x, point.y);
}

bool EditorWindow::in_other_text_body(POINT point) const
{
    if (controller_.command_palette_active() || controller_.command_line_active() ||
        controller_.explorer_focused() || previewing())
    {
        return false;
    }
    const auto other = core::other_pane_layout(body_panes(controller_.delivery().settings),
                                               controller_.split().active_side);
    return other.has_value() && core::contains(other.value().content, point.x, point.y);
}

void EditorWindow::press_right(LPARAM data)
{
    const auto point = point_of(data);
    // 開いているメニューの外の右の押しは閉じてから、新しい点の押しとして覚える（中は何もしない）。
    if (controller_.context_menu_active())
    {
        if (renderer_ != nullptr && renderer_->context_menu_covers(point.x, point.y))
        {
            right_pressed_ = std::nullopt;
            return;
        }
        send(application::CloseContextMenu{});
    }
    right_pressed_ =
        in_text_body(point) || in_other_text_body(point) ? std::optional{point} : std::nullopt;
}

void EditorWindow::release_right(LPARAM data)
{
    const auto pressed = std::exchange(right_pressed_, std::nullopt);
    const auto point = point_of(data);
    if (!pressed.has_value())
    {
        return;
    }
    // もう 1 つの枠の上なら、左クリックと同じくその枠へ移ってから開く（ADR 0148 の決定 11）。
    if (in_other_text_body(point))
    {
        send(application::SwitchPane{});
    }
    if (!in_text_body(point))
    {
        return;
    }
    send(application::OpenContextMenu{
        point.x, point.y,
        text_point(point, core::SelectionAnchoring::collapse, application::PointerClicks::single)});
}

// lParam が -1 は鍵から（Shift+F10・メニューキー・DefWindowProc が送る）。開いたときだけ真。本文の
// 右クリックは窓が右の離しを受けて DefWindowProc に渡さないので WM_CONTEXTMENU は来ず、2 回開かない
// （来ても lParam が -1 でないので開かず、既定の処理へ渡る）。
bool EditorWindow::open_context_menu_from_keys(LPARAM data)
{
    if (!opened_from_keys(data) || controller_.command_palette_active() ||
        controller_.command_line_active() || controller_.explorer_focused())
    {
        return false;
    }
    const auto body = active_body(controller_.delivery().settings);
    POINT at{body.content.left, body.content.top};
    if (renderer_ != nullptr)
    {
        const RECT caret = renderer_->caret_rectangle();
        at = POINT{caret.left, caret.bottom};
    }
    send(application::OpenContextMenu{at.x, at.y, std::nullopt});
    return true;
}

// メニューの上の左の押しは覚えるだけ（離したときに行を実行する）。外の押しは閉じるだけで、本文へ
// 渡さない（Windows のメニューと同じ）。
bool EditorWindow::press_context_menu(LPARAM data)
{
    if (!controller_.context_menu_active())
    {
        return false;
    }
    const auto point = point_of(data);
    context_menu_pressed_ =
        renderer_ != nullptr && renderer_->context_menu_covers(point.x, point.y);
    if (!context_menu_pressed_)
    {
        send(application::CloseContextMenu{});
    }
    return true;
}

bool EditorWindow::release_context_menu(LPARAM data)
{
    if (!std::exchange(context_menu_pressed_, false) || !controller_.context_menu_active())
    {
        return false;
    }
    const auto point = point_of(data);
    const auto row =
        renderer_ == nullptr ? std::nullopt : renderer_->context_menu_row_at(point.x, point.y);
    if (row.has_value())
    {
        send(application::ChooseContextMenu{row});
    }
    return true;
}

// 載せた行が変わったときだけ送る（同じ行の上の動きで描き直さない・frame を作らない）。
bool EditorWindow::point_context_menu(LPARAM data)
{
    if (!controller_.context_menu_active())
    {
        return false;
    }
    const auto point = point_of(data);
    const auto row =
        renderer_ == nullptr ? std::nullopt : renderer_->context_menu_row_at(point.x, point.y);
    const auto shown = controller_.context_menu_highlighted();
    if (row != shown && (row.has_value() || shown.has_value()))
    {
        send(application::PointContextMenu{row});
    }
    return true;
}

// 開かなかった WM_CONTEXTMENU（帯の空きの右クリックの窓のメニュー・面や入力行の間の Shift+F10）は
// 既定の処理へ渡す。
LRESULT EditorWindow::context_menu_message(UINT message, WPARAM word, LPARAM data)
{
    switch (message)
    {
    case WM_RBUTTONDOWN:
        press_right(data);
        return 0;
    case WM_RBUTTONUP:
        release_right(data);
        return 0;
    // Shift+F10 の後の SC_KEYMENU を捨てる（ContextMenuKey.hpp）。ほかの WM_SYSCOMMAND は既定へ。
    case WM_SYSCOMMAND:
        return swallows_key_menu(word, data, controller_.context_menu_active(), held(VK_SHIFT))
                   ? 0
                   : DefWindowProcW(window_, message, word, data);
    default:
        return open_context_menu_from_keys(data) ? 0 : DefWindowProcW(window_, message, word, data);
    }
}

void EditorWindow::close_context_menu_outside()
{
    if (controller_.context_menu_active())
    {
        send(application::CloseContextMenu{});
    }
}

bool EditorWindow::take_context_menu_key(UINT message, WPARAM word)
{
    if (!controller_.context_menu_active())
    {
        return false;
    }
    switch (message)
    {
    case WM_KEYDOWN:
        press_context_menu_key(word);
        return true;
    case WM_CHAR:
        return true;
    case WM_KILLFOCUS:
        send(application::CloseContextMenu{});
        return false;
    default:
        return false;
    }
}

// 開いている間の鍵は上下・Enter・Esc だけで、ほかは捨てる（文字も本文へ入れない）。OS の仮想キーは
// 開いた集合（CPP-017）。
void EditorWindow::press_context_menu_key(WPARAM word)
{
    switch (word)
    {
    case VK_UP:
        send(application::StepContextMenu{false});
        return;
    case VK_DOWN:
        send(application::StepContextMenu{true});
        return;
    case VK_RETURN:
        send(application::ChooseContextMenu{std::nullopt});
        return;
    case VK_ESCAPE:
        send(application::CloseContextMenu{});
        return;
    default:
        return;
    }
}
} // namespace nenenib::ui::win32
