#pragma once

#include <windows.h>

namespace nenenib::ui::win32
{
// 窓の class の CS_DBLCLKS（エクスプローラーの面のダブルクリック・ADR 0162 の E4 の追補）で、
// 同じボタンの 2 回目の押しは *BUTTONDBLCLK で届く。面の上の左のダブルクリックのほかは、1 回目と
// 同じ押しとして扱う（中ボタンの続けての「タブを閉じる」・ADR 0056 の決定 9）。OS のメッセージ
// 番号は開いた集合なので既定分岐を置く（CPP-017）。
[[nodiscard]] constexpr UINT single_press_of(UINT message) noexcept
{
    switch (message)
    {
    case WM_LBUTTONDBLCLK:
        return WM_LBUTTONDOWN;
    case WM_MBUTTONDBLCLK:
        return WM_MBUTTONDOWN;
    case WM_RBUTTONDBLCLK:
        return WM_RBUTTONDOWN;
    default:
        return message;
    }
}
} // namespace nenenib::ui::win32
