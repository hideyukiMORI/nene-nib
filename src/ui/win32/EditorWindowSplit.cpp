// EditorWindow の 2 枠の境界のドラッグ（ADR 0148 の 2026-11-22 の追補・#1941）。エクスプローラーの
// 面の幅のドラッグ（ADR 0162 の追補 D78・EditorWindowExplorer.cpp）と同じ形で、押した点から動いたと
// きだけ境界の比を意図 ResizeSplit で送る。比が変われば deliver の follow_split が操作中の枠の寸法
// を送り直す（窓の大きさを変えたときと同じ道）。覚えの次の値と送る比は split_step の 1 本
// （SplitPress.hpp・窓の試験 --split-press）、比の規則と範囲は core の split_share_at の 1 本。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "LayoutRect.hpp"
#include "PaneLayouts.hpp"
#include "PaneSplit.hpp"
#include "ResizeSplit.hpp"
#include "SplitExtent.hpp"
#include "SplitLayout.hpp"
#include "SplitPoint.hpp"
#include "SplitPress.hpp"
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

// 点の下の見えている境界の始まり（split_divider_start_under）。1 枠なら配置を計算せず値なし（マウス
// の動くたびに通るので frame も delivery も作らない・split の 1 欄と字の大きさの 1 欄を読むだけ）。
// Ctrl+P の一覧と全面プレビューの間は境界を出さない。
std::optional<std::int32_t> EditorWindow::split_divider_start(LPARAM point) const
{
    const auto layout = controller_.split().layout;
    if (layout == core::SplitLayout::none)
    {
        return std::nullopt;
    }
    const bool hidden = controller_.command_palette_active() || full_previewing();
    return split_divider_start_under(hidden ? std::nullopt
                                            : std::optional{body_panes(controller_.font_size())},
                                     layout, dpi_, POINT{low_word_of(point), high_word_of(point)});
}

// 窓のメッセージ 1 通を split_step に流し、覚えを写して比を返す。境界の始まりは押しとダブルクリック
// だけ、寸法は押している間の動きと離しだけ引く（押していない WM_MOUSEMOVE で配置を計算しない）。
std::optional<core::SplitShare> EditorWindow::step_split(UINT message, LPARAM data)
{
    const bool pressing = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK;
    const bool following =
        split_grab_.has_value() && (message == WM_MOUSEMOVE || message == WM_LBUTTONUP);
    SplitPoint point{split_coordinate_of(controller_.split().layout,
                                         POINT{low_word_of(data), high_word_of(data)}),
                     pressing ? split_divider_start(data) : std::nullopt, std::nullopt,
                     controller_.split().share};
    if (following)
    {
        point.extent = core::split_extent(pane_client(), dpi_, controller_.font_size(),
                                          controller_.pane_split());
    }
    auto [next, share] = split_step(message, point, split_grab_);
    split_grab_ = next;
    return share;
}

// 境界の押し。本文の押しより先に引く（境界の両側は枠の端の行と重なる）。押しただけでは送らない。
// 当たりはカーソルとダブルクリックと同じ split_divider_start（全面プレビューと Ctrl+P の一覧の間は
// 隠れた境界を拾わず、押しはプレビューのリンクや選択へ届く・レビュー R4 の M1）。
bool EditorWindow::press_split_divider(LPARAM data)
{
    if (!split_divider_start(data).has_value())
    {
        return false;
    }
    SetCapture(window_);
    if (GetCapture() == window_)
    {
        static_cast<void>(step_split(WM_LBUTTONDOWN, data));
        split_drag_point_ = data;
    }
    return true;
}

// 押したまま動かした。押した点から動いたときだけ、比が変わったら送る（controller は状態に置くだけ・
// 寸法の送り直しは follow_split）。
void EditorWindow::drag_split_divider(LPARAM data)
{
    if (!split_grab_.has_value())
    {
        return;
    }
    split_drag_point_ = data;
    const auto share = step_split(WM_MOUSEMOVE, data);
    if (share.has_value())
    {
        send(application::ResizeSplit{share.value()});
    }
}

// 離した。動かしていれば離した点の比を送る。境界を押していなければ偽。
bool EditorWindow::release_split_divider(LPARAM data)
{
    if (!split_grab_.has_value())
    {
        return false;
    }
    // 覚えは step_split が消す（捕まえを放すと WM_CAPTURECHANGED が lose_split_divider へ戻るので、
    // 放す前に消しておく）。
    const auto share = step_split(WM_LBUTTONUP, data);
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    if (share.has_value())
    {
        send(application::ResizeSplit{share.value()});
    }
    settle_split_divider(data);
    return true;
}

// 離す前に捕まえを失った（別の窓へ移った・Alt+Tab）。比は送った所のまま。印だけ消す。
void EditorWindow::lose_split_divider()
{
    if (!split_grab_.has_value())
    {
        return;
    }
    static_cast<void>(step_split(WM_CAPTURECHANGED, 0));
    left_pressed_.reset();
    over_split_divider_ = false;
}

// ドラッグ中の Esc（面の境と同じ・Windows の分割バーの慣習）。押す前の比へ戻してドラッグを終える。
// 境界を押していなければ偽で、Esc は鍵の道へ流れる。押していればその 1 回の Esc を食べる。
bool EditorWindow::cancel_split_divider()
{
    if (!split_grab_.has_value())
    {
        return false;
    }
    const auto share = step_split(WM_KEYDOWN, split_drag_point_);
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    if (share.has_value())
    {
        send(application::ResizeSplit{share.value()});
    }
    // 離すまで待たず、戻った境界と最後の点でカーソルを決め直す（面の境の cancel と同じ）。
    settle_split_divider(split_drag_point_);
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
    over_split_divider_ = split_divider_start(point).has_value();
    SetCursor(LoadCursorW(
        nullptr, over_split_divider_ ? divider_cursor_of(controller_.split().layout) : IDC_ARROW));
}

// 境界のダブルクリックは等分へ戻す。境界の上でなければ偽。
bool EditorWindow::reset_split_share(LPARAM data)
{
    const auto share = step_split(WM_LBUTTONDBLCLK, data);
    if (!share.has_value())
    {
        return false;
    }
    send(application::ResizeSplit{share.value()});
    return true;
}

// WM_MOUSEMOVE の点が境界に載ったか・外れたか。変わったときだけカーソルを置き直す。ドラッグの間と、
// ほかの要素を押している間は変えない（面の境と同じ）。
void EditorWindow::hover_split_divider(LPARAM data)
{
    if (split_grab_.has_value() || explorer_grab_offset_.has_value() || selecting_ ||
        preview_dragging_ || explorer_pressed_ || left_pressed_.has_value() ||
        minimap_grab_.has_value())
    {
        return;
    }
    const bool over = split_divider_start(data).has_value();
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
