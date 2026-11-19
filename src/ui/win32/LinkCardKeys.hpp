#pragma once

#include <windows.h>

namespace nenenib::ui::win32
{
// 札を Enter で開いて閉じた後の、同じ Enter の押し続けの反復か（#1876 のレビューの軽い 5）。held は
// 札の Enter を受けてからその Enter がまだ離されていないこと、data は WM_KEYDOWN の lParam（bit 30
// が「前から押されていた」）。真なら窓は鍵を捨てる（本文に改行を入れない）。
[[nodiscard]] constexpr bool link_card_enter_repeat(bool held, WPARAM word, LPARAM data) noexcept
{
    constexpr LPARAM previously_down = LPARAM{1} << 30U;
    return held && word == VK_RETURN && (data & previously_down) != 0;
}
} // namespace nenenib::ui::win32
