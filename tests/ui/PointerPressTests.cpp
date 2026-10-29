// #997 の独立レビュー R2-1: 窓の class の CS_DBLCLKS で 2 回目の押しが *BUTTONDBLCLK で届いても、
// 中ボタン（と右・面の外の左）は 1 回目と同じ押しとして扱う（ADR 0056 の決定 9・ADR 0162 の E4 の
// 追補）。窓は作らず、窓の手続きが押しを写す 1 本（single_press_of）を確かめる。
#include "PointerPress.hpp"
#include "WindowChecks.hpp"

#include <array>
#include <cstddef>

namespace nenenib::tests::ui
{
void verify_pointer_presses()
{
    using nenenib::ui::win32::single_press_of;
    expect(single_press_of(WM_MBUTTONDBLCLK) == WM_MBUTTONDOWN &&
               single_press_of(WM_RBUTTONDBLCLK) == WM_RBUTTONDOWN &&
               single_press_of(WM_LBUTTONDBLCLK) == WM_LBUTTONDOWN,
           "a second press of a button is the same press as the first");
    expect(single_press_of(WM_MBUTTONDOWN) == WM_MBUTTONDOWN &&
               single_press_of(WM_MBUTTONUP) == WM_MBUTTONUP &&
               single_press_of(WM_MOUSEMOVE) == WM_MOUSEMOVE,
           "other messages pass through unchanged");
    // 中ボタンを 2 回続けて押して離す（OS は 2 回目を DBLCLK で送る）と、押しが 2 回届く
    // ＝帯のタブを 2 つ続けて閉じられる。
    constexpr std::array<UINT, 4> twice{WM_MBUTTONDOWN, WM_MBUTTONUP, WM_MBUTTONDBLCLK,
                                        WM_MBUTTONUP};
    std::size_t presses = 0;
    for (const UINT message : twice)
    {
        presses += single_press_of(message) == WM_MBUTTONDOWN ? 1 : 0;
    }
    expect(presses == 2, "two quick middle clicks are two presses (two tabs close)");
}
} // namespace nenenib::tests::ui
