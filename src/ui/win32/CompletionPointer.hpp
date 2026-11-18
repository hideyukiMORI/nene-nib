#pragma once

#include "PointerClicks.hpp"

#include <windows.h>

namespace nenenib::ui::win32
{
// 補完の面のマウスの守り（#1844 の再確認 R1b・L1b・L3b）。窓を作らずに試せるようここに置く
// （nib_window_tests の `--completion`）。

// 面の上で始まった押しの列（ダブル・トリプルクリック）の 2 回目・3 回目の押しは本文へ渡さない。
// 1 回目の離しで確定して面が閉じた後の押しが、面の下にあった本文の語を選ばない（起動画面の
// ADR 0172 の追補 24 と同じ形）。began は列の 1 回目の押しが面の上だったこと。
[[nodiscard]] constexpr bool completion_swallows_press(application::PointerClicks clicks,
                                                       bool began) noexcept
{
    return began && clicks != application::PointerClicks::single;
}

// 面の上で押した行の覚えを下ろすメッセージ。面の押しは捕まえを取らないので、窓の外で離すと離しが
// 届かない。窓の外へ出た・捕まえが変わったときに下ろし、次の離しを呑まない。
[[nodiscard]] constexpr bool completion_press_lost(UINT message) noexcept
{
    return message == WM_MOUSELEAVE || message == WM_CAPTURECHANGED;
}

// 面の上の右ボタン・中ボタンの押しは面が受ける（面を閉じるだけで、本文へ渡さない）。active は面が
// 見えていること、covered は押した点が面の上にあること。
[[nodiscard]] constexpr bool completion_takes_button(bool active, bool covered) noexcept
{
    return active && covered;
}
} // namespace nenenib::ui::win32
