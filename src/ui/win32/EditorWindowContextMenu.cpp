// 本文の右クリックのメニューの窓の側（ADR 0078 の追補 #1051・D71・FR-031）。右クリック・
// Shift+F10・メニューキーで開き、開いている間の鍵とマウスはメニューへ送る。何を出すか・何で閉じる
// か・選んだ行で何をするかは controller が決め、窓は意図を送るだけ（ARC-011）。当たりは renderer が
// 最後に描いた配置（core::context_menu_layout の 1 本）を引く。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ChooseContextMenu.hpp"
#include "CloseContextMenu.hpp"
#include "LayoutRect.hpp"
#include "OpenContextMenu.hpp"
#include "PointContextMenu.hpp"
#include "PointerClicks.hpp"
#include "SelectionAnchoring.hpp"
#include "StepContextMenu.hpp"

#include <windows.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
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
    right_pressed_ = in_text_body(point) ? std::optional{point} : std::nullopt;
}

void EditorWindow::release_right(LPARAM data)
{
    const auto pressed = std::exchange(right_pressed_, std::nullopt);
    const auto point = point_of(data);
    if (!pressed.has_value() || !in_text_body(point))
    {
        return;
    }
    send(application::OpenContextMenu{
        point.x, point.y,
        text_point(point, core::SelectionAnchoring::collapse, application::PointerClicks::single)});
}

// lParam が -1 は鍵から（Shift+F10・メニューキー・DefWindowProc が送る）。マウスの WM_CONTEXTMENU
// は右の離しを窓が受けて DefWindowProc に渡さないので来ない（来ても開かない）。
void EditorWindow::open_context_menu_from_keys(LPARAM data)
{
    if (data != -1 || controller_.command_palette_active() || controller_.command_line_active() ||
        controller_.explorer_focused())
    {
        return;
    }
    const auto body = active_body(controller_.delivery().settings);
    POINT at{body.content.left, body.content.top};
    if (renderer_ != nullptr)
    {
        const RECT caret = renderer_->caret_rectangle();
        at = POINT{caret.left, caret.bottom};
    }
    send(application::OpenContextMenu{at.x, at.y, std::nullopt});
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

LRESULT EditorWindow::context_menu_message(UINT message, LPARAM data)
{
    switch (message)
    {
    case WM_RBUTTONDOWN:
        press_right(data);
        break;
    case WM_RBUTTONUP:
        release_right(data);
        break;
    default:
        open_context_menu_from_keys(data);
        break;
    }
    return 0;
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
