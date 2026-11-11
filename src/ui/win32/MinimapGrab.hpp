#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// ミニマップの面を押してから離すまでの覚え（ADR 0171 の決定 5 の追補）。panel_top は押したときの
// 面の上端（物理画素・ドラッグの間は面を引き直さない）、row は最後に送った面の段（同じ段の
// WM_MOUSEMOVE を送らない）。どちらも単独で妥当な値なので公開 aggregate（CPP-003）。
struct MinimapGrab
{
    std::int32_t panel_top;
    std::int32_t row;
};

[[nodiscard]] constexpr bool operator==(const MinimapGrab &left, const MinimapGrab &right) noexcept
{
    return left.panel_top == right.panel_top && left.row == right.row;
}
} // namespace nenenib::ui::win32
