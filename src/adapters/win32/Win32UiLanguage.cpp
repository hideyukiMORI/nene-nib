#include "Win32UiLanguage.hpp"

#include <windows.h>

namespace nenenib::adapters::win32
{
std::optional<core::DisplayLanguage> display_language_of(std::uint16_t language_id) noexcept
{
    const auto primary = PRIMARYLANGID(language_id);
    if (primary == LANG_JAPANESE)
    {
        return core::DisplayLanguage::japanese;
    }
    const auto sub = SUBLANGID(language_id);
    if (primary == LANG_CHINESE && (sub == SUBLANG_CHINESE_SIMPLIFIED ||
                                    sub == SUBLANG_CHINESE_SINGAPORE || sub == SUBLANG_NEUTRAL))
    {
        return core::DisplayLanguage::simplified_chinese;
    }
    return core::DisplayLanguage::english;
}

std::optional<core::DisplayLanguage> Win32UiLanguage::observe()
{
    return display_language_of(GetUserDefaultUILanguage());
}
} // namespace nenenib::adapters::win32
