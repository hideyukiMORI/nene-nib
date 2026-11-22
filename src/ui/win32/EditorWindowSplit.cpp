// EditorWindow の 2 枠の境界のドラッグ（ADR 0148 の 2026-11-22 の追補・#1941）。エクスプローラーの
// 面の幅のドラッグ（ADR 0162 の追補 D78・EditorWindowExplorer.cpp）と同じ形で、押した点から動いたと
// きだけ境界の比を意図 ResizeSplit で送る。比が変われば deliver の follow_split が操作中の枠の寸法
// を送り直す（窓の大きさを変えたときと同じ道）。比の規則と範囲は core の split_share_at の 1 本。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "LayoutRect.hpp"
#include "PaneLayouts.hpp"
#include "PaneSplit.hpp"
#include "ResizeSplit.hpp"
#include "SplitExtent.hpp"
#include "SplitLayout.hpp"
#include "SplitShare.hpp"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
using detail::high_word_of;
using detail::low_word_of;

// 境界の上のカーソル。左右の分割は左右の矢印、上下の分割は上下の矢印。
[[nodiscard]] LPCWSTR divider_cursor_of(core::SplitLayout layout) noexcept
{
    switch (layout)
    {
    case core::SplitLayout::horizontal:
        return IDC_SIZENS;
    case core::SplitLayout::none:
    case core::SplitLayout::vertical:
        return IDC_SIZEWE;
    }
    std::unreachable();
}
} // namespace

// 点が境界の上か（境界の両側 3 DIP・面の境と同じ広さ）。1 枠なら配置を計算せず偽（マウスの動く
// たびに通るので frame も delivery も作らない・split の 1 欄と字の大きさの 1 欄を読むだけ）。
bool EditorWindow::split_divider_at(std::int32_t x, std::int32_t y) const
{
    if (controller_.split().layout == core::SplitLayout::none)
    {
        return false;
    }
    return core::split_divider_hit(body_panes(controller_.font_size()), dpi_, x, y);
}

// 点の分割の向きに沿った座標（左右なら x・上下なら y）。
std::int32_t EditorWindow::split_coordinate(LPARAM data) const
{
    switch (controller_.split().layout)
    {
    case core::SplitLayout::horizontal:
        return high_word_of(data);
    case core::SplitLayout::none:
    case core::SplitLayout::vertical:
        return low_word_of(data);
    }
    std::unreachable();
}

// 境界の始まり（左か上の端）を点 coordinate に置いたときの比。押した点と境界の始まりのずれを引く。
std::optional<core::SplitShare> EditorWindow::split_share_at(std::int32_t coordinate) const
{
    const auto split = controller_.pane_split();
    const auto extent = core::split_extent(pane_client(), dpi_, controller_.font_size(), split);
    if (!extent.has_value())
    {
        return std::nullopt;
    }
    return core::split_share_at(extent.value(), coordinate - split_grab_offset_.value_or(0));
}

// 境界の押し。本文の押しより先に引く（境界の両側は枠の端の行と重なる）。押しただけでは送らない。
bool EditorWindow::press_split_divider(LPARAM data)
{
    if (!split_divider_at(low_word_of(data), high_word_of(data)))
    {
        return false;
    }
    const auto divider = body_panes(controller_.font_size()).divider;
    SetCapture(window_);
    if (GetCapture() == window_ && divider.has_value())
    {
        const auto coordinate = split_coordinate(data);
        const bool across = controller_.split().layout == core::SplitLayout::horizontal;
        split_grab_offset_ = coordinate - (across ? divider.value().top : divider.value().left);
        split_grab_at_ = coordinate;
        split_dragged_ = false;
        split_pressed_share_ = controller_.split().share;
    }
    return true;
}

// 押したまま動かした。押した点から動いたときだけ、比が変わったら送る（controller は状態に置くだけ・
// 寸法の送り直しは follow_split）。
void EditorWindow::drag_split_divider(LPARAM data)
{
    const auto coordinate = split_coordinate(data);
    if (!split_grab_offset_.has_value() || (!split_dragged_ && coordinate == split_grab_at_))
    {
        return;
    }
    split_dragged_ = true;
    const auto share = split_share_at(coordinate);
    if (share.has_value() && !(share.value() == controller_.split().share))
    {
        send(application::ResizeSplit{share.value()});
    }
}

// 離した。動かしていれば離した点の比を送る。境界を押していなければ偽。
bool EditorWindow::release_split_divider(LPARAM data)
{
    if (!split_grab_offset_.has_value())
    {
        return false;
    }
    drag_split_divider(data);
    // 捕まえを放すと WM_CAPTURECHANGED が lose_split_divider へ戻るので、先に印を消す。
    split_grab_offset_.reset();
    split_dragged_ = false;
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    settle_split_divider(data);
    return true;
}

// 離す前に捕まえを失った（別の窓へ移った・Alt+Tab）。比は送った所のまま。印だけ消す。
void EditorWindow::lose_split_divider()
{
    if (!std::exchange(split_grab_offset_, std::nullopt).has_value())
    {
        return;
    }
    split_dragged_ = false;
    left_pressed_.reset();
    over_split_divider_ = false;
}

// ドラッグ中の Esc（面の境と同じ・Windows の分割バーの慣習）。押す前の比へ戻してドラッグを終える。
// 境界を押していなければ偽で、Esc は鍵の道へ流れる。押していればその 1 回の Esc を食べる。
bool EditorWindow::cancel_split_divider()
{
    if (!std::exchange(split_grab_offset_, std::nullopt).has_value())
    {
        return false;
    }
    const bool dragged = std::exchange(split_dragged_, false);
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    if (dragged)
    {
        send(application::ResizeSplit{split_pressed_share_});
    }
    over_split_divider_ = false;
    return true;
}

// WM_KEYDOWN の Esc（take_explorer_escape と同じ形）。取り消した Esc を押し続けた繰り返し（bit 30）
// は離して押し直すまで食べる。食べたら真。
bool EditorWindow::take_split_escape(LPARAM data)
{
    constexpr LPARAM repeated_key_bit = LPARAM{1} << 30;
    if ((data & repeated_key_bit) != 0 && split_escape_held_)
    {
        return true;
    }
    split_escape_held_ = cancel_split_divider();
    return split_escape_held_;
}

// 境界の捕まえが終わった。押しで置いた帯の印を消し、カーソルを点で今決め直す（捕まえの間は
// WM_SETCURSOR が来ない）。
void EditorWindow::settle_split_divider(LPARAM point)
{
    left_pressed_.reset();
    over_split_divider_ = split_divider_under(point);
    SetCursor(LoadCursorW(
        nullptr, over_split_divider_ ? divider_cursor_of(controller_.split().layout) : IDC_ARROW));
}

// 点が境界の上か（Ctrl+P の一覧と全面プレビューの間は境界を出さない）。
bool EditorWindow::split_divider_under(LPARAM point) const
{
    return !controller_.command_palette_active() && !full_previewing() &&
           split_divider_at(low_word_of(point), high_word_of(point));
}

// 境界のダブルクリックは等分へ戻す。境界の上でなければ偽。
bool EditorWindow::reset_split_share(LPARAM data)
{
    if (!split_divider_under(data))
    {
        return false;
    }
    send(application::ResizeSplit{core::SplitShare{}});
    return true;
}

// WM_MOUSEMOVE の点が境界に載ったか・外れたか。変わったときだけカーソルを置き直す。ドラッグの間と、
// ほかの要素を押している間は変えない（面の境と同じ）。
void EditorWindow::hover_split_divider(LPARAM data)
{
    if (split_grab_offset_.has_value() || explorer_grab_offset_.has_value() || selecting_ ||
        preview_dragging_ || explorer_pressed_ || left_pressed_.has_value() ||
        minimap_grab_.has_value())
    {
        return;
    }
    const bool over = split_divider_under(data);
    if (over == std::exchange(over_split_divider_, over))
    {
        return;
    }
    SetCursor(
        LoadCursorW(nullptr, over ? divider_cursor_of(controller_.split().layout) : IDC_ARROW));
}

// WM_SETCURSOR。client の上で最後の WM_MOUSEMOVE が境界に載っていれば矢印を保つ。ほかは偽。
bool EditorWindow::split_divider_cursor(LPARAM data) const
{
    if (LOWORD(data) != HTCLIENT || !over_split_divider_)
    {
        return false;
    }
    SetCursor(LoadCursorW(nullptr, divider_cursor_of(controller_.split().layout)));
    return true;
}
// 2 つの境（面の境と枠の境界）のカーソル・捕まえ・載せの印をまとめて扱う口（pointer_message の
// 分岐を短く保つ）。
bool EditorWindow::divider_cursor(LPARAM data) const
{
    return explorer_divider_cursor(data) || split_divider_cursor(data);
}

void EditorWindow::lose_dividers()
{
    lose_explorer_divider();
    lose_split_divider();
}

void EditorWindow::forget_divider_hover() noexcept
{
    over_explorer_divider_ = false;
    over_split_divider_ = false;
}
} // namespace nenenib::ui::win32
