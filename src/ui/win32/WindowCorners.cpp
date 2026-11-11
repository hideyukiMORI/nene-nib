#include "WindowCorners.hpp"

namespace nenenib::ui::win32
{
HRESULT square_window_corners(HWND window) noexcept
{
    const DWM_WINDOW_CORNER_PREFERENCE preference = window_corner_preference;
    return DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &preference,
                                 static_cast<DWORD>(sizeof(preference)));
}
} // namespace nenenib::ui::win32
