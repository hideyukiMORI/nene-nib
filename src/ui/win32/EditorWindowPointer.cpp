#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "ExplorerKey.hpp"
#include "ExplorerKeyKind.hpp"
#include "FontSizeAdjustment.hpp"
#include "NewTab.hpp"
#include "PaletteLayout.hpp"
#include "PaneLayouts.hpp"
#include "PointTitleBar.hpp"
#include "PointerPress.hpp"
#include "ScrollPaletteList.hpp"
#include "ScrollPane.hpp"
#include "ScrollTabs.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "StatusBarLayout.hpp"
#include "SwitchPane.hpp"
#include "SwitchTab.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
using detail::held;
using detail::high_word_of;
using detail::low_word_of;

namespace
{
constexpr std::int32_t resize_border_dips = 8;

// 縁のヒットテストは 3 行 3 列の表で引く。分岐で書くと CPP-012 の認知的複雑度を越える。
constexpr std::array<LRESULT, 9> border_codes{HTNOWHERE, HTLEFT,       HTRIGHT,
                                              HTTOP,     HTTOPLEFT,    HTTOPRIGHT,
                                              HTBOTTOM,  HTBOTTOMLEFT, HTBOTTOMRIGHT};

[[nodiscard]] bool caption_button(WPARAM word) noexcept
{
    return word == HTMINBUTTON || word == HTMAXBUTTON || word == HTCLOSE;
}

[[nodiscard]] bool window_button(core::TitleBarHit hit) noexcept
{
    return hit == core::TitleBarHit::minimize || hit == core::TitleBarHit::maximize ||
           hit == core::TitleBarHit::close;
}

[[nodiscard]] LRESULT border_hit(POINT point, const RECT &client, std::int32_t margin) noexcept
{
    const std::size_t row = point.y < margin ? 1U : (point.y >= client.bottom - margin ? 2U : 0U);
    const std::size_t column = point.x < margin ? 1U : (point.x >= client.right - margin ? 2U : 0U);
    return border_codes.at(row * 3U + column);
}

[[nodiscard]] LRESULT caption_code(core::TitleBarHit hit) noexcept
{
    switch (hit)
    {
    case core::TitleBarHit::minimize:
        return HTMINBUTTON;
    case core::TitleBarHit::maximize:
        return HTMAXBUTTON;
    case core::TitleBarHit::close:
        return HTCLOSE;
    case core::TitleBarHit::caption:
        return HTCAPTION;
    case core::TitleBarHit::tab:
    case core::TitleBarHit::tab_close:
    case core::TitleBarHit::add_tab:
    case core::TitleBarHit::tab_list:
    case core::TitleBarHit::none:
        return HTCLIENT;
    }
    std::unreachable();
}
} // namespace

LRESULT EditorWindow::pointer_message(UINT message, WPARAM word, LPARAM data)
{
    // マウスの 7 通。帯の上の要素は同じ 1 本の title_bar_target で引く（ADR 0056 の決定 9）。
    switch (message)
    {
    case WM_LBUTTONDOWN:
        click_client(data);
        return 0;
    case WM_LBUTTONDBLCLK:
        // 面の上だけがダブルクリック。ほかは 2 回目の押しとして今までどおり（CS_DBLCLKS）。
        if (!double_click_explorer(data))
        {
            return pointer_message(single_press_of(message), word, data);
        }
        return 0;
    case WM_LBUTTONUP:
        drag_selection(data);
        end_selection_drag();
        release_preview_task(data);
        release_title_bar(data);
        release_explorer(data);
        return 0;
    case WM_MBUTTONDOWN:
        press_middle(data);
        return 0;
    case WM_MBUTTONUP:
        middle_click(data);
        return 0;
    case WM_MOUSEMOVE:
        drag_selection(data);
        point_at(data);
        return 0;
    case WM_MOUSELEAVE:
        hover(std::nullopt);
        return 0;
    case WM_MOUSEWHEEL:
        turn_wheel(word, data);
        return 0;
    case WM_CAPTURECHANGED:
        end_selection_drag();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

core::TitleBarLayout EditorWindow::title_bar() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::title_bar_layout(controller_.title_bar_input(client.right, dpi_));
}

core::TitleBarTarget EditorWindow::title_bar_target_at(LPARAM data) const
{
    return core::title_bar_target(title_bar(), low_word_of(data), high_word_of(data));
}

bool EditorWindow::click_title_bar(LPARAM data)
{
    const auto target = title_bar_target_at(data);
    left_pressed_ = target;
    switch (target.hit)
    {
    case core::TitleBarHit::tab:
        send(application::SwitchTab{target.tab});
        return true;
    // ×・「＋」・「∨」は離したときに動かす（窓の操作と同じ）。
    case core::TitleBarHit::tab_close:
    case core::TitleBarHit::add_tab:
    case core::TitleBarHit::tab_list:
        return true;
    case core::TitleBarHit::caption:
    case core::TitleBarHit::minimize:
    case core::TitleBarHit::maximize:
    case core::TitleBarHit::close:
    case core::TitleBarHit::none:
        return false;
    }
    std::unreachable();
}

void EditorWindow::release_title_bar(LPARAM data)
{
    // 押した要素と離した要素が同じときだけ動かす。押して帯が送られ、同じ点へ来た × では閉じない。
    const auto pressed = std::exchange(left_pressed_, std::nullopt);
    if (controller_.command_palette_active())
    {
        return;
    }
    const auto target = core::title_bar_released(pressed, title_bar_target_at(data));
    if (!target.has_value())
    {
        return;
    }
    if (target.value().hit == core::TitleBarHit::tab_close)
    {
        close_tab(target.value().tab);
    }
    if (target.value().hit == core::TitleBarHit::add_tab)
    {
        send(application::NewTab{});
    }
    // 「∨」は Ctrl+P の面にタブの一覧を開く（ADR 0057 の決定 7）。面が開いている間は上で
    // 帯のクリックを受けないので、閉じるのは Esc と面の外のクリック（今の Ctrl+P と同じ）。
    if (target.value().hit == core::TitleBarHit::tab_list)
    {
        send(application::OpenTabList{});
    }
}

void EditorWindow::press_middle(LPARAM data)
{
    middle_pressed_ = std::nullopt;
    if (controller_.command_palette_active())
    {
        return;
    }
    middle_pressed_ = title_bar_target_at(data);
}

void EditorWindow::middle_click(LPARAM data)
{
    const auto pressed = std::exchange(middle_pressed_, std::nullopt);
    if (controller_.command_palette_active())
    {
        return;
    }
    const auto target = core::title_bar_released(pressed, title_bar_target_at(data));
    if (!target.has_value())
    {
        return;
    }
    if (target.value().hit == core::TitleBarHit::tab ||
        target.value().hit == core::TitleBarHit::tab_close)
    {
        close_tab(target.value().tab);
    }
}

void EditorWindow::point_at(LPARAM data)
{
    // 窓の外や caption（非クライアント領域）へ出たとき WM_MOUSELEAVE が来るように頼む。
    TRACKMOUSEEVENT track{};
    track.cbSize = static_cast<DWORD>(sizeof(track));
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = window_;
    TrackMouseEvent(&track);
    hover(core::title_bar_hover(title_bar_target_at(data)));
}

void EditorWindow::hover(const std::optional<core::TitleBarTarget> &target)
{
    // 前の値は application の状態が持つ。変わったときだけ意図を送る（ADR 0056 の決定 3）。
    RECT client{};
    GetClientRect(window_, &client);
    if (controller_.title_bar_input(client.right, dpi_).hovered == target)
    {
        return;
    }
    send(application::PointTitleBar{target});
}

LRESULT EditorWindow::hit_test(LPARAM data) noexcept
{
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    RECT client{};
    GetClientRect(window_, &client);
    const auto hit = core::title_bar_target(title_bar(), point.x, point.y).hit;
    // 窓の操作の上では大きさを変えられない。それ以外の縁は 8 DIP を 8 方向に割り当てる。
    if (window_button(hit))
    {
        return caption_code(hit);
    }
    const LRESULT border = border_hit(point, client, core::to_pixels(resize_border_dips, dpi_));
    if (border != HTNOWHERE)
    {
        return border;
    }
    return caption_code(hit);
}

LRESULT EditorWindow::press_caption(UINT message, WPARAM word, LPARAM data) noexcept
{
    if (!caption_button(word))
    {
        return DefWindowProcW(window_, message, word, data);
    }
    // 押下は飲み込み、離したときに動かす。DefWindowProcW に任せると OS の描画が混ざる。
    if (message == WM_NCLBUTTONUP)
    {
        activate_caption(word);
    }
    return 0;
}

void EditorWindow::activate_caption(WPARAM word) noexcept
{
    if (word == HTMINBUTTON)
    {
        ShowWindow(window_, SW_MINIMIZE);
    }
    if (word == HTMAXBUTTON)
    {
        ShowWindow(window_, IsZoomed(window_) != 0 ? SW_RESTORE : SW_MAXIMIZE);
    }
    if (word == HTCLOSE)
    {
        // 閉じる経路は WM_CLOSE 1 本（ARC-001）。未保存の確認もそこで 1 度だけ起きる。
        SendMessageW(window_, WM_CLOSE, 0, 0);
    }
}

void EditorWindow::click_client(LPARAM data)
{
    left_pressed_ = std::nullopt;
    preview_task_press_.forget();
    if (controller_.command_palette_active())
    {
        click_palette(data);
        return;
    }
    if (click_title_bar(data))
    {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const auto layout = core::status_bar_layout(client.right, client.bottom, dpi_);
    const bool in_status = core::contains(layout.band, low_word_of(data), high_word_of(data));
    if (controller_.command_line_active() && in_status)
    {
        return;
    }
    if (controller_.command_line_active())
    {
        send(application::CancelCommand{});
    }
    if (controller_.delivery().command_message.has_value() && in_status)
    {
        send(application::CancelCommand{});
        return;
    }
    if (press_explorer(data))
    {
        return;
    }
    switch (core::status_bar_hit(layout, low_word_of(data), high_word_of(data)))
    {
    // 意図は「どちらを選んだか」。同じ側を押しても controller が同じ表示値を返すだけ（ARC-011）。
    case core::StatusBarHit::toggle_ordinary:
        send(application::SelectEditMode{core::EditMode::ordinary});
        return;
    case core::StatusBarHit::toggle_vim:
        send(application::SelectEditMode{core::EditMode::vim});
        return;
    case core::StatusBarHit::none:
        break;
    }
    click_body(data);
}

// もう 1 つの枠を押したら先にその枠へ移り、移った後の配置で同じ点を解く（ADR 0148 の決定 11）。
// プレビューの枠は移る先ではないので、押しても何もしない（本文のキャレットも動かさない・ADR
// 0155）。
void EditorWindow::click_body(LPARAM data)
{
    // 面に焦点がある間に本文を押したら焦点を本文へ戻す。面の Esc と同じ意図（ADR 0162 の追補）。
    if (controller_.explorer_focused())
    {
        send(application::ExplorerKey{application::ExplorerKeyKind::esc});
    }
    // 全面プレビューの間は本文の面の全部がプレビューで、押すのはチェックボックスか描いた文字の
    // 選択（本文のキャレットも枠も動かさない・ADR 0155 の改訂 15・18・追補 24）。
    if (full_previewing())
    {
        press_preview_task(data);
        if (!preview_task_press_.pressed())
        {
            press_preview_text(data);
        }
        return;
    }
    const auto settings = controller_.delivery().settings;
    const auto other =
        core::other_pane_layout(body_panes(settings), controller_.split().active_side);
    if (other.has_value() &&
        core::contains(other.value().band, low_word_of(data), high_word_of(data)))
    {
        if (previewing())
        {
            press_preview_task(data);
            return;
        }
        send(application::SwitchPane{});
    }
    if (core::contains(active_body(settings).band, low_word_of(data), high_word_of(data)))
    {
        place_caret(data);
        begin_selection_drag();
    }
}

void EditorWindow::click_palette(LPARAM data)
{
    RECT client{};
    GetClientRect(window_, &client);
    const auto view = controller_.command_palette_view();
    if (!view.has_value())
    {
        return;
    }
    const auto &palette = view.value();
    const auto layout = core::palette_layout(client.right, client.bottom, dpi_, palette.total);
    const auto hit = core::palette_hit(layout, low_word_of(data), high_word_of(data));
    if (hit.has_value())
    {
        send(application::ActivateCommandChoice{palette.first + hit.value()});
        return;
    }
    if (!core::contains(layout.panel, low_word_of(data), high_word_of(data)))
    {
        send(application::CancelCommand{});
    }
}

void EditorWindow::place_caret(LPARAM data)
{
    const auto anchoring =
        held(VK_SHIFT) ? core::SelectionAnchoring::extend : core::SelectionAnchoring::collapse;
    point_text(POINT{low_word_of(data), high_word_of(data)}, anchoring);
}

std::int32_t EditorWindow::title_bar_width() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::to_dips(client.right, dpi_);
}

void EditorWindow::turn_wheel(WPARAM word, LPARAM data)
{
    const auto delta = static_cast<std::int16_t>(HIWORD(word));
    // 面の上のホイールは一覧だけを送り、選択は動かさない（ADR 0150 の決定 2）。
    if (controller_.command_palette_active())
    {
        scroll_palette(delta);
        return;
    }
    // 面を閉じた後のホイールは面の余りを持ち越さない（#737）。
    palette_wheel_remainder_ = 0;
    if (over_title_bar(data))
    {
        scroll_tabs(delta);
        return;
    }
    if ((LOWORD(word) & MK_CONTROL) != 0)
    {
        zoom_wheel(delta);
        return;
    }
    zoom_wheel_remainder_ = 0;
    // エクスプローラーの面の上は面の行だけを送る（ADR 0162 の決定 4・wheel_turned の累積）。
    if (over_explorer(data))
    {
        scroll_explorer(delta);
        return;
    }
    explorer_wheel_remainder_ = 0;
    scroll_body(delta, data);
}

void EditorWindow::scroll_palette(std::int32_t delta)
{
    // 本文と同じく delta を累積して行に変える（#737）。1 刻みの行数は OS の設定から
    // （1 画面の設定なら面の見えている行数・#499）。
    std::size_t visible = 0;
    if (wheel_setting_ == WHEEL_PAGESCROLL)
    {
        RECT client{};
        GetClientRect(window_, &client);
        visible = core::palette_window_rows(client.right, client.bottom, dpi_);
    }
    const auto turn = wheel_turned(palette_wheel_remainder_, delta,
                                   wheel_lines_per_notch(wheel_setting_, visible));
    palette_wheel_remainder_ = turn.remainder;
    if (turn.lines == 0)
    {
        return;
    }
    send(application::ScrollPaletteList{turn.lines});
}

void EditorWindow::scroll_body(std::int32_t delta, LPARAM data)
{
    // 1 刻みの行数は OS の設定から。1 画面の設定なら見えている行数（#499）。
    std::size_t visible = 0;
    if (wheel_setting_ == WHEEL_PAGESCROLL)
    {
        RECT client{};
        GetClientRect(window_, &client);
        visible = core::body_layout(client.right, client.bottom, dpi_,
                                    controller_.delivery().settings.font_size)
                      .visible_lines;
    }
    const auto turn =
        wheel_turned(body_wheel_remainder_, delta, wheel_lines_per_notch(wheel_setting_, visible));
    body_wheel_remainder_ = turn.remainder;
    if (turn.lines == 0)
    {
        return;
    }
    // 点の下の枠を送る。もう 1 つの枠なら枠を移らずに送る（ADR 0148 の決定 11）。
    if (over_other_pane(data))
    {
        send(application::ScrollPane{turn.lines});
        return;
    }
    send(application::ScrollLines{turn.lines});
}

void EditorWindow::read_wheel_setting()
{
    // 起動と WM_SETTINGCHANGE で読む 1 か所。読めなければ Windows の既定の 3 行。値が変わったとき
    // だけ単位が変わるので前の余りを捨てる（ほかの設定の変更でも WM_SETTINGCHANGE は来る）。
    UINT lines = default_wheel_scroll_lines;
    if (SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0) == FALSE)
    {
        lines = default_wheel_scroll_lines;
    }
    if (lines != wheel_setting_)
    {
        body_wheel_remainder_ = 0;
        palette_wheel_remainder_ = 0;
    }
    wheel_setting_ = lines;
}

bool EditorWindow::over_other_pane(LPARAM data) const
{
    // WM_MOUSEWHEEL の位置は画面の座標なので、クライアントの座標に直してから見る。
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    const auto other = core::other_pane_layout(body_panes(controller_.delivery().settings),
                                               controller_.split().active_side);
    return other.has_value() && core::contains(other.value().band, point.x, point.y);
}

bool EditorWindow::over_title_bar(LPARAM data) const
{
    // WM_MOUSEWHEEL の位置は画面の座標なので、クライアントの座標に直してから帯の上かを見る。
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    return core::title_bar_target(title_bar(), point.x, point.y).hit != core::TitleBarHit::none;
}

void EditorWindow::scroll_tabs(std::int32_t delta)
{
    // 刻みに満たない分は次へ繰り越す（高分解能のホイール）。奥が正・手前が負（ADR 0056 の決定 3）。
    tab_wheel_remainder_ += delta;
    const std::int32_t notches = tab_wheel_remainder_ / WHEEL_DELTA;
    tab_wheel_remainder_ %= WHEEL_DELTA;
    if (notches != 0)
    {
        send(application::ScrollTabs{notches});
    }
}

void EditorWindow::zoom_wheel(std::int32_t delta)
{
    zoom_wheel_remainder_ += delta;
    const std::int32_t steps = zoom_wheel_remainder_ / WHEEL_DELTA;
    zoom_wheel_remainder_ %= WHEEL_DELTA;
    if (steps == 0)
    {
        return;
    }
    const auto adjustment =
        steps > 0 ? core::FontSizeAdjustment::increase : core::FontSizeAdjustment::decrease;
    const auto amount = static_cast<std::size_t>(steps > 0 ? steps : -steps);
    send(application::AdjustFontSize{adjustment, amount});
}
} // namespace nenenib::ui::win32
