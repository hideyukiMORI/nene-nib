#pragma once

#include <windows.h>

#include <dwmapi.h>

namespace nenenib::ui::win32
{
// 窓の角の好み（#1488・ADR 0008 の決定 9 の追記）。Windows 11 は既定で最上位の窓の角を丸め、丸い
// 角の弧と client の四角い絵の境が左上と右上に小さく明るい点として見えた。施主の指示で角は四角。
// 値は Windows SDK の dwmapi.h の DWM_WINDOW_CORNER_PREFERENCE（DWMWCP_DONOTROUND = 1）。
inline constexpr DWM_WINDOW_CORNER_PREFERENCE window_corner_preference = DWMWCP_DONOTROUND;

// window の角を四角にするよう DWM へ伝える 1 本（DWMWA_WINDOW_CORNER_PREFERENCE）。呼ぶのは
// EditorWindow::apply_backdrop の 1 か所で、DWMWA_USE_IMMERSIVE_DARK_MODE と同じく戻り値は
// 見ない（Windows 10 と Windows 11 の最初の版には属性が無く失敗を返すだけ・角は今までどおり）。
// 戻り値は試験が「試みた」ことを確かめるために返す。最大化・Snap・影・リサイズの縁は変えない。
[[nodiscard]] HRESULT square_window_corners(HWND window) noexcept;
} // namespace nenenib::ui::win32
