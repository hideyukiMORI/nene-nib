// プレビューの枠より広い表の横の送りの窓の側（#1883・施主決定 D107 の案 A・ADR 0151 の
// 2026-11-19 の追補）。点の下の表と寸法は renderer の当たり（PreviewPane::table_at / table_bar）で
// 引き、ScrollPreviewTable と HoverPreviewTable を送るだけ。送りの量・つかみ・端への寄せ・溝の
// 押しの 1 枠は controller が決める（ARC-011）。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "Direct2DRenderer.hpp"
#include "HoverPreviewTable.hpp"
#include "LinkCardTarget.hpp"
#include "PreviewTableGesture.hpp"
#include "PreviewTableHit.hpp"
#include "ScrollPreviewTable.hpp"
#include "SettingsPart.hpp"

#include <windows.h>
#include <windowsx.h>

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
namespace
{
// 横の送りの 1 刻み（WHEEL_DELTA）の量は 40 DIP（3 単位で 1 DIP・刻みに満たない量は繰り越す）。
constexpr std::int32_t wheel_units_per_dip = WHEEL_DELTA / 40;
} // namespace

// 棒の上の左の押し。点の下の広い表の棒の帯なら、つまみをつかむか溝を 1 枠送り、捕まえを取る。
bool EditorWindow::press_table_bar(LPARAM data)
{
    const auto hit = renderer_ == nullptr
                         ? std::nullopt
                         : renderer_->preview_table_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
    if (!hit.has_value() || !hit.value().bar || window_ == nullptr)
    {
        return false;
    }
    send(application::ScrollPreviewTable{hit.value().table, application::PreviewTableGesture::press,
                                         hit.value().at, hit.value().span});
    SetCapture(window_);
    if (GetCapture() == window_)
    {
        table_dragging_ = hit.value().table;
    }
    return true;
}

// つかんだ間の動き（つかんだ表の溝の上の横の位置だけを送る）。つかんでいなければ偽。
bool EditorWindow::drag_table_bar(LPARAM data)
{
    if (!table_dragging_.has_value())
    {
        return false;
    }
    const auto hit = renderer_ == nullptr ? std::nullopt
                                          : renderer_->preview_table_bar(table_dragging_.value(),
                                                                         GET_X_LPARAM(data));
    if (hit.has_value())
    {
        send(application::ScrollPreviewTable{hit.value().table,
                                             application::PreviewTableGesture::drag, hit.value().at,
                                             hit.value().span});
    }
    return true;
}

// 離しと捕まえの喪失。捕まえを放すと WM_CAPTURECHANGED がここへ戻るので、先に印を消す。
bool EditorWindow::release_table_bar()
{
    if (!table_dragging_.has_value())
    {
        return false;
    }
    const auto table = table_dragging_.value();
    table_dragging_.reset();
    if (GetCapture() == window_)
    {
        ReleaseCapture();
    }
    send(application::ScrollPreviewTable{table, application::PreviewTableGesture::release, 0,
                                         core::PreviewTableSpan{0, 0}});
    return true;
}

// 横の量 delta（WHEEL_DELTA の単位・右が正）のホイール。点（画面の座標）の下に広い表があれば
// その表を送り、端に着いていても受ける（縦へ流さない）。表の外なら偽で、余りを捨てる。
bool EditorWindow::wheel_table(std::int32_t delta, LPARAM data)
{
    POINT point{GET_X_LPARAM(data), GET_Y_LPARAM(data)};
    ScreenToClient(window_, &point);
    const auto hit = renderer_ == nullptr || faces_cover(point.x, point.y)
                         ? std::nullopt
                         : renderer_->preview_table_at(point.x, point.y);
    if (!hit.has_value())
    {
        table_wheel_remainder_ = 0;
        return false;
    }
    table_wheel_remainder_ += delta;
    const auto dips = table_wheel_remainder_ / wheel_units_per_dip;
    table_wheel_remainder_ %= wheel_units_per_dip;
    if (dips != 0)
    {
        send(application::ScrollPreviewTable{
            hit.value().table, application::PreviewTableGesture::wheel, dips, hit.value().span});
    }
    return true;
}

// マウスを載せた棒（つかんでいる間・選択のドラッグの間・面が開いている間は値なし）。
void EditorWindow::hover_table_bar(LPARAM data)
{
    std::optional<std::size_t> table;
    if (renderer_ != nullptr && !table_dragging_.has_value() && !selecting_ && !preview_dragging_ &&
        !faces_cover(GET_X_LPARAM(data), GET_Y_LPARAM(data)))
    {
        const auto hit = renderer_->preview_table_at(GET_X_LPARAM(data), GET_Y_LPARAM(data));
        if (hit.has_value() && hit.value().bar)
        {
            table = hit.value().table;
        }
    }
    hover_table_bar_of(table);
}

bool EditorWindow::faces_cover(std::int32_t x, std::int32_t y) const
{
    if (controller_.command_palette_active())
    {
        return true;
    }
    if (renderer_ == nullptr)
    {
        return false;
    }
    return settings_target_under(x, y).part != core::SettingsPart::outside ||
           (controller_.completion_active() && renderer_->completion_covers(x, y)) ||
           renderer_->context_menu_covers(x, y) || renderer_->hover_tip_covers(x, y) ||
           renderer_->link_card_target_at(x, y) != LinkCardTarget::outside;
}

// 前の値は application の状態が持つ。変わったときだけ意図を送る（HoverExplorer と同じ）。
void EditorWindow::hover_table_bar_of(std::optional<std::size_t> table)
{
    if (controller_.preview_table_hovered() == table)
    {
        return;
    }
    send(application::HoverPreviewTable{table});
}
} // namespace nenenib::ui::win32
