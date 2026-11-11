#pragma once

#include "ExitPlan.hpp"

#include <windows.h>

#include <dwmapi.h>

#include <utility>

namespace nenenib::ui::win32
{
// 描画を失って取消の無い「描画できなくなったので閉じます。保存しますか。」を聞く間だけの窓の下地
// （#1499・ADR 0133 の追補・ADR 0008 の決定 9 の追記）。
// 窓の Mica は #1488 で外したので、描けない窓は縁と影だけの見えない矩形になり、
// 箱だけが宙に浮いて見えた。WS_EX_NOREDIRECTIONBITMAP の窓では GDI の塗りも
// class の背景ブラシも画面に出ないので、下地は DWM の Mica だけ。
// 窓はこの後 leave で必ず壊れるので外さない（角の約 4 px の点はこの非常時だけ受ける・#1496）。
inline constexpr DWM_SYSTEMBACKDROP_TYPE question_backdrop = DWMSBT_MAINWINDOW;

// 終わり方の扱いのうち、下地を掛けるのは取消の無い箱（描画を失った終わり）だけ。recover() が
// 成功した道と、確かめるものが無くすぐ終わる道では掛けない。
[[nodiscard]] constexpr bool needs_question_backdrop(application::ExitPlan plan) noexcept
{
    switch (plan)
    {
    case application::ExitPlan::confirm_each_tab_without_cancel:
        return true;
    case application::ExitPlan::confirm_each_tab:
    case application::ExitPlan::block_session_end:
    case application::ExitPlan::leave_now:
        return false;
    }
    std::unreachable();
}

// window に question_backdrop を掛けるよう DWM へ伝える 1 本。呼ぶのは EditorWindow::end_window の
// 1 か所で、失敗でも箱へ進む（Windows 10 と 11 21H2 には属性が無く失敗を返すだけ）。
// 戻り値は試験が「試みた」ことを確かめるために返す。
[[nodiscard]] HRESULT show_question_backdrop(HWND window) noexcept;
} // namespace nenenib::ui::win32
