// プレビューのリンクの札の窓の側（施主決定 D105・D106・#1869 の段 L2）。
// プレビューの文字の上の押しと離しの文字の位置を renderer の当たり
// （PreviewPane::character_at）で引いて ClickPreviewLink を送る。
// 札が出ている間の Enter・Esc・Ctrl+C と札のボタンは ActOnLinkCard にする。
// どの文字がリンクか・宛先の種類・何で閉じるかは controller が決め、窓は意図を送るだけ（ARC-011）。
// ほかの鍵は受けずに今までの道へ通す（controller がその意図の前に札を閉じる）。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ActOnLinkCard.hpp"
#include "ClickPreviewLink.hpp"
#include "Direct2DRenderer.hpp"
#include "LinkCardAction.hpp"
#include "LinkCardKeys.hpp"
#include "LinkCardTarget.hpp"

#include <windows.h>
#include <windowsx.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
using detail::held;

// 札が出ている間の Enter・Esc・Ctrl+C を札が受けたか。修飾の違う鍵とほかの鍵は受けない。
// Enter・Esc・Ctrl+C の後の WM_CHAR は制御文字なので type_character が捨てる。
bool EditorWindow::take_link_card_key(UINT message, WPARAM word, LPARAM data)
{
    if (follow_link_card_enter(message, word, data))
    {
        return true;
    }
    if (message != WM_KEYDOWN || !controller_.link_card_active() || held(VK_MENU))
    {
        return false;
    }
    return press_link_card_key(word);
}

// 札の Enter の押し続けの反復なら真（捨てる）。離したら覚えを下ろし、ほかの押しでも下ろす（#1876 の
// レビューの軽い 5）。
bool EditorWindow::follow_link_card_enter(UINT message, WPARAM word, LPARAM data)
{
    if (message == WM_KEYUP && word == VK_RETURN)
    {
        link_card_enter_held_ = false;
        return false;
    }
    if (message != WM_KEYDOWN)
    {
        return false;
    }
    link_card_enter_held_ = link_card_enter_repeat(link_card_enter_held_, word, data);
    return link_card_enter_held_;
}

bool EditorWindow::press_link_card_key(WPARAM word)
{
    const bool control = held(VK_CONTROL);
    const bool shift = held(VK_SHIFT);
    if (word == VK_RETURN && !control && !shift)
    {
        link_card_enter_held_ = true;
        send(application::ActOnLinkCard{application::LinkCardAction::open});
        return true;
    }
    if (word == VK_ESCAPE && !control && !shift)
    {
        send(application::ActOnLinkCard{application::LinkCardAction::close});
        return true;
    }
    if (word == 'C' && control && !shift)
    {
        send(application::ActOnLinkCard{application::LinkCardAction::copy});
        return true;
    }
    return false;
}

// 札の上の押しは部品を覚えるだけ（本文へ渡さない）。札の外の押しは札を閉じてから今までの道へ。
bool EditorWindow::press_link_card(LPARAM data)
{
    link_card_pressed_.reset();
    if (renderer_ == nullptr || !controller_.link_card_active())
    {
        return false;
    }
    const auto target = renderer_->link_card_target_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
    if (target == LinkCardTarget::outside)
    {
        send(application::ActOnLinkCard{application::LinkCardAction::close});
        return false;
    }
    link_card_pressed_ = target;
    return true;
}

// 札の上で押した離しはその部品だけ。押したボタンと離したボタンが同じときだけ送る。
bool EditorWindow::release_link_card(LPARAM data)
{
    const auto pressed = std::exchange(link_card_pressed_, std::nullopt);
    if (!pressed.has_value())
    {
        return false;
    }
    const auto released =
        renderer_ == nullptr || !controller_.link_card_active()
            ? LinkCardTarget::outside
            : renderer_->link_card_target_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
    if (released == pressed.value() && released == LinkCardTarget::primary)
    {
        send(application::ActOnLinkCard{application::LinkCardAction::primary});
    }
    if (released == pressed.value() && released == LinkCardTarget::secondary)
    {
        send(application::ActOnLinkCard{application::LinkCardAction::secondary});
    }
    return true;
}

// プレビューの文字の上の押しの位置を覚える（チェックボックスの押しと同じく、離すまで送らない）。
void EditorWindow::press_preview_link(LPARAM data)
{
    preview_link_pressed_ =
        renderer_ == nullptr
            ? std::nullopt
            : renderer_->preview_character_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
}

// 押しと離しの文字が両方あれば送る（同じリンクかは controller が決める）。面（Ctrl+P）が開いている
// 間は送らない。押しはここで必ず消える。
void EditorWindow::release_preview_link(LPARAM data)
{
    const auto pressed = std::exchange(preview_link_pressed_, std::nullopt);
    if (!pressed.has_value() || renderer_ == nullptr || !previewing() ||
        controller_.command_palette_active())
    {
        return;
    }
    const auto released = renderer_->preview_character_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
    if (released.has_value())
    {
        send(application::ClickPreviewLink{pressed.value(), released.value()});
    }
}
} // namespace nenenib::ui::win32
