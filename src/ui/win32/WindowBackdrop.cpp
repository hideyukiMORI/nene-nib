#include "WindowBackdrop.hpp"

namespace nenenib::ui::win32
{
HRESULT show_question_backdrop(HWND window) noexcept
{
    const DWM_SYSTEMBACKDROP_TYPE backdrop = question_backdrop;
    return DwmSetWindowAttribute(window, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                                 static_cast<DWORD>(sizeof(backdrop)));
}
} // namespace nenenib::ui::win32
