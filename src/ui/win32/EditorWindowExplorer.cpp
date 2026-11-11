// EditorWindow のエクスプローラーの面の群（ADR 0162 の決定 4・ADR 0158 の追補 7）。面に焦点が
// あるときの鍵の写し・面の上の押した行と離した行とダブルクリック・ホイール・面と本文の境の
// ドラッグ（追補 D78）。面の状態と規則は application が持ち、ここは窓の入力を意図へ写すだけ。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ExplorerKey.hpp"
#include "ExplorerKeyKind.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerPointer.hpp"
#include "ExplorerPointing.hpp"
#include "ExplorerResizing.hpp"
#include "ExplorerView.hpp"
#include "LayoutRect.hpp"
#include "PointExplorer.hpp"
#include "ResizeExplorer.hpp"
#include "ScrollExplorer.hpp"
#include "WheelTurn.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
using application::ExplorerKeyKind;
using detail::high_word_of;
using detail::low_word_of;

// 面に焦点があるときの鍵の表（決定 4）。表にない鍵は捨てる（Vim の表と `map` の表を通さない）。
constexpr std::array<std::pair<WPARAM, ExplorerKeyKind>, 10> explorer_keys{{
    {'J', ExplorerKeyKind::j},
    {'K', ExplorerKeyKind::k},
    {'H', ExplorerKeyKind::h},
    {'L', ExplorerKeyKind::l},
    {VK_RETURN, ExplorerKeyKind::enter},
    {VK_ESCAPE, ExplorerKeyKind::esc},
    {VK_DOWN, ExplorerKeyKind::down},
    {VK_UP, ExplorerKeyKind::up},
    {VK_LEFT, ExplorerKeyKind::left},
    {VK_RIGHT, ExplorerKeyKind::right},
}};

[[nodiscard]] std::optional<ExplorerKeyKind> explorer_key_for(WPARAM key) noexcept
{
    for (const auto &[virtual_key, kind] : explorer_keys)
    {
        if (virtual_key == key)
        {
            return kind;
        }
    }
    return std::nullopt;
}
} // namespace

// 面に焦点があるときの鍵（WM_KEYDOWN）。操作の鍵（Ctrl+P・F1・Ctrl+Shift+E など）は press_key が
// 先に受けるので、ここへ来るのはそれ以外の鍵だけ。文字の鍵の WM_CHAR は type_character が捨てる。
void EditorWindow::press_explorer_key(WPARAM word)
{
    const auto kind = explorer_key_for(word);
    if (kind.has_value())
    {
        send(application::ExplorerKey{kind.value()});
    }
}

// 点の下の面の行（木の行の位置）。面の外なら値なし。面の先頭の行（ルート名）と行の無い所は
// 木の行の数（行の外・controller は何もしない・押した印は消す）。
std::optional<std::size_t> EditorWindow::explorer_row_at(LPARAM data) const
{
    const auto layout = explorer_frame(controller_.font_size());
    const auto x = low_word_of(data);
    const auto y = high_word_of(data);
    if (!layout.panel.has_value() || !core::contains(layout.panel.value(), x, y) ||
        layout.line_height <= 0)
    {
        return std::nullopt;
    }
    const auto view = controller_.explorer_view();
    if (!view.has_value())
    {
        return std::nullopt;
    }
    const auto line = static_cast<std::size_t>((y - layout.panel.value().top) / layout.line_height);
    if (line == 0 || line > view.value().rows.size())
    {
        return view.value().total;
    }
    return view.value().first_visible + line - 1;
}

// 面の行の上の左ボタン（ADR 0056 の決定 9・ADR 0162 の決定 4 の改め #1388）。押しと DBLCLK は
// 面の上の行（ルート名と行の無い所は行の外）を、離しは押した印が立っているときだけ離した点の行
// （面の外なら行の外・controller が押した印を消す）を送る。写しは explorer_pointing_of の 1 本で、
// クリックとダブルクリックの意味は controller が決める。面の上で受けたら真。
bool EditorWindow::point_explorer(UINT message, LPARAM data)
{
    const auto pointing = explorer_pointing_of(message, explorer_pressed_);
    if (!pointing.has_value())
    {
        return false;
    }
    auto row = explorer_row_at(data);
    if (pointing.value() == application::ExplorerPointing::released)
    {
        explorer_pressed_ = false;
        const auto view = controller_.explorer_view();
        if (!view.has_value())
        {
            return false;
        }
        row = row.value_or(view.value().total);
    }
    if (!row.has_value())
    {
        return false;
    }
    explorer_pressed_ = explorer_pressed_after(pointing.value());
    send(application::PointExplorer{row.value(), pointing.value()});
    return true;
}

void EditorWindow::scroll_explorer(std::int32_t delta)
{
    // 1 刻みの行数は OS の設定から。1 画面の設定なら本文の見えている行数。
    std::size_t visible = 0;
    if (wheel_setting_ == WHEEL_PAGESCROLL)
    {
        visible = active_body(controller_.delivery().settings).visible_lines;
    }
    const auto turn = wheel_turned(explorer_wheel_remainder_, delta,
                                   wheel_lines_per_notch(wheel_setting_, visible));
    explorer_wheel_remainder_ = turn.remainder;
    if (turn.lines == 0)
    {
        return;
    }
    send(application::ScrollExplorer{turn.lines});
}

// ---------------------------------------------------------------- 境のドラッグ（ADR 0162 の追補
// D78）

// 点が面と本文の境（左右 3 DIP）の上か。面が消えていれば設定を読まずに偽（マウスの動くたびに
// 通るので、frame を作らない・面が消えている道は controller の 1 欄を読むだけ）。
bool EditorWindow::explorer_divider_at(std::int32_t x, std::int32_t y) const
{
    if (!controller_.explorer_width().has_value())
    {
        return false;
    }
    return core::explorer_divider_hit(explorer_frame(controller_.font_size()), dpi_, x, y);
}

// 面の右端を点 x に置いたときに覚える幅（DIP）。押した点と右端のずれを引く。
std::int32_t EditorWindow::explorer_width_at(std::int32_t x) const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::explorer_dragged_width(client.right, dpi_, x - explorer_grab_offset_.value_or(0));
}

// 境の押し。面の行の押しより先に引く（境は面の右端の行と重なる）。押しただけでは何も送らない。
bool EditorWindow::press_explorer_divider(LPARAM data)
{
    const auto x = low_word_of(data);
    if (!explorer_divider_at(x, high_word_of(data)))
    {
        return false;
    }
    const auto panel = explorer_frame(controller_.font_size()).panel;
    SetCapture(window_);
    if (GetCapture() == window_ && panel.has_value())
    {
        explorer_grab_offset_ = x - panel.value().right;
        explorer_grab_x_ = x;
        explorer_dragged_.reset();
    }
    return true;
}

// 押したまま動かした。押した点から動いたときだけ幅を送り、本文の寸法を送り直す（窓の寸法が変わった
// ときと同じ道・設定には書かない）。
void EditorWindow::drag_explorer_divider(LPARAM data)
{
    const auto x = low_word_of(data);
    if (!explorer_grab_offset_.has_value() ||
        (!explorer_dragged_.has_value() && x == explorer_grab_x_))
    {
        return;
    }
    const auto width = explorer_width_at(x);
    if (explorer_dragged_ == width)
    {
        return;
    }
    explorer_dragged_ = width;
    send(application::ResizeExplorer{application::ExplorerResizing::dragged, width});
    send(body_view());
}

// 離した。動かしていれば離した点の幅を送る（controller が設定へ 1
// 回書く）。境を押していなければ偽。
bool EditorWindow::release_explorer_divider(LPARAM data)
{
    if (!explorer_grab_offset_.has_value())
    {
        return false;
    }
    const bool moved = std::exchange(explorer_dragged_, std::nullopt).has_value() ||
                       low_word_of(data) != explorer_grab_x_;
    const auto width = explorer_width_at(low_word_of(data));
    // 捕まえを放すと WM_CAPTURECHANGED がここへ戻るので、先に印を消す。
    explorer_grab_offset_.reset();
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    if (moved)
    {
        send(application::ResizeExplorer{application::ExplorerResizing::dragged, width});
        send(application::ResizeExplorer{application::ExplorerResizing::released, width});
        send(body_view());
    }
    return true;
}

// 離す前に捕まえを失った（別の窓へ移った・Alt+Tab）。最後に送った幅を離したのと同じに送る。
void EditorWindow::lose_explorer_divider()
{
    if (!std::exchange(explorer_grab_offset_, std::nullopt).has_value())
    {
        return;
    }
    const auto dragged = std::exchange(explorer_dragged_, std::nullopt);
    if (dragged.has_value())
    {
        send(application::ResizeExplorer{application::ExplorerResizing::released, dragged.value()});
        send(body_view());
    }
}

// ドラッグ中の Esc（Issue #1413・Windows の分割バーの慣習）。押す前の幅へ戻してドラッグを終える
// （controller はドラッグの幅を捨て、設定に書かない）。境を押していなければ偽で、Esc は鍵の道へ
// 流れる。押していればその 1 回の Esc を食べる（本文・Vim・面へ流さない）。捕まえを放すと
// WM_CAPTURECHANGED が lose_explorer_divider へ戻るので、先に印を消す。
bool EditorWindow::cancel_explorer_divider()
{
    if (!std::exchange(explorer_grab_offset_, std::nullopt).has_value())
    {
        return false;
    }
    const auto dragged = std::exchange(explorer_dragged_, std::nullopt);
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    if (dragged.has_value())
    {
        send(application::ResizeExplorer{application::ExplorerResizing::cancelled, 0});
        send(body_view());
    }
    return true;
}

// ドラッグの途中で面が閉じた（ToggleExplorer・`:set noexplorer` など・follow_explorer が呼ぶ）。
// ドラッグは終わりで、幅は送らない（controller もドラッグの幅を捨て、設定は覚えた値のまま・
// Issue #1380 の設計席の決め）。捕まえを放すと WM_CAPTURECHANGED が lose_explorer_divider へ戻る
// ので、先に印を消す。境のカーソルの印も消す（面が無いので境も無い）。
void EditorWindow::end_hidden_explorer_drag()
{
    over_explorer_divider_ = false;
    if (!std::exchange(explorer_grab_offset_, std::nullopt).has_value())
    {
        return;
    }
    explorer_dragged_.reset();
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
}

// 境のダブルクリックは既定の幅（今の式）へ戻す。境の上でなければ偽。
bool EditorWindow::reset_explorer_width(LPARAM data)
{
    if (controller_.command_palette_active() ||
        !explorer_divider_at(low_word_of(data), high_word_of(data)))
    {
        return false;
    }
    send(application::ResizeExplorer{application::ExplorerResizing::reset, 0});
    send(body_view());
    return true;
}

// WM_MOUSEMOVE の点が境に載ったか・外れたか。変わったときだけカーソルを置き直す（点はメッセージの
// 値から読み、OS のカーソルの位置は読まない・ARC-007）。ドラッグの間は捕まえているので変えない。
// ほかの要素を押している間（本文・全面プレビューの選択のドラッグ・面の行・帯）も、境を横切って
// カーソルを変えない（Issue #1380 の R-3）。
void EditorWindow::hover_explorer_divider(LPARAM data)
{
    if (explorer_grab_offset_.has_value() || selecting_ || preview_dragging_ || explorer_pressed_ ||
        left_pressed_.has_value())
    {
        return;
    }
    const bool over = !controller_.command_palette_active() &&
                      explorer_divider_at(low_word_of(data), high_word_of(data));
    if (over == std::exchange(over_explorer_divider_, over))
    {
        return;
    }
    SetCursor(LoadCursorW(nullptr, over ? IDC_SIZEWE : IDC_ARROW));
}

// WM_SETCURSOR。client の上で最後の WM_MOUSEMOVE が境に載っていれば左右の矢印を保つ（ドラッグの
// 間は捕まえているので WM_SETCURSOR は来ない）。ほかは偽（DefWindowProcW が窓の class のカーソルを
// 置く）。
bool EditorWindow::explorer_divider_cursor(LPARAM data) const
{
    if (LOWORD(data) != HTCLIENT || !over_explorer_divider_)
    {
        return false;
    }
    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    return true;
}

bool EditorWindow::over_explorer(LPARAM data) const
{
    // WM_MOUSEWHEEL の位置は画面の座標なので、クライアントの座標に直してから見る。
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    const auto panel = explorer_frame(controller_.font_size()).panel;
    return panel.has_value() && core::contains(panel.value(), point.x, point.y);
}
} // namespace nenenib::ui::win32
