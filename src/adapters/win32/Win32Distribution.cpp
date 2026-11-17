#include "Win32Distribution.hpp"

#include <windows.h>

#include <appmodel.h>

namespace nenenib::adapters::win32
{
core::Distribution distribution_of(std::int32_t package_name_result) noexcept
{
    return package_name_result == APPMODEL_ERROR_NO_PACKAGE ? core::Distribution::unpackaged
                                                            : core::Distribution::packaged;
}

core::Distribution current_distribution() noexcept
{
    // 名前は要らないので置き場を渡さず、長さだけを問う（identity が在れば置き場が足りないと返る）。
    UINT32 length = 0;
    return distribution_of(GetCurrentPackageFullName(&length, nullptr));
}
} // namespace nenenib::adapters::win32
