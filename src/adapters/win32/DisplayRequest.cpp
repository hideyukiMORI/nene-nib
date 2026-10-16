#include "DisplayRequest.hpp"

#include <windows.h>

#include <array>
#include <string_view>

namespace nenenib::adapters::win32
{
bool grid_display_requested()
{
    constexpr wchar_t variable[] = L"NIB_DISPLAY";
    std::array<wchar_t, 8> value{};
    const DWORD copied =
        GetEnvironmentVariableW(variable, value.data(), static_cast<DWORD>(value.size()));
    return copied > 0 && copied < value.size() &&
           std::wstring_view(value.data(), copied) == L"grid";
}
} // namespace nenenib::adapters::win32
