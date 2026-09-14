#include "Win32AppearanceAdapter.hpp"

#include <windows.h>

namespace nenenib::adapters::win32
{
namespace
{
constexpr wchar_t personalize_key[] =
    LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)";
constexpr wchar_t light_value[] = L"AppsUseLightTheme";
} // namespace

std::expected<core::Appearance, application::AppearanceReadFailure>
Win32AppearanceAdapter::current() const noexcept
{
    DWORD light = 0;
    DWORD size = sizeof(light);
    const auto status = RegGetValueW(HKEY_CURRENT_USER, personalize_key, light_value,
                                     RRF_RT_REG_DWORD, nullptr, &light, &size);
    // キーも値も同じ ERROR_FILE_NOT_FOUND で返る。どちらも「設定が無い」であって壊れてはいない。
    if (status == ERROR_FILE_NOT_FOUND)
    {
        return std::unexpected(application::AppearanceReadFailure::unavailable);
    }
    if (status != ERROR_SUCCESS)
    {
        return std::unexpected(application::AppearanceReadFailure::unreadable);
    }
    return light == 0 ? core::Appearance::dark : core::Appearance::light;
}
} // namespace nenenib::adapters::win32
