#include "Win32Distribution.hpp"

#include <windows.h>

#include <appmodel.h>

namespace nenenib::adapters::win32
{
core::Distribution distribution_of(std::int32_t package_name_result) noexcept
{
    // 分からない値は今までどおり（ZIP 版）に倒す。誤って packaged にすると効く機能が消える。
    return package_name_result == ERROR_INSUFFICIENT_BUFFER || package_name_result == ERROR_SUCCESS
               ? core::Distribution::packaged
               : core::Distribution::unpackaged;
}

core::Distribution current_distribution() noexcept
{
    // 名前は要らないので置き場を渡さず、長さだけを問う（identity が在れば置き場が足りないと返る）。
    UINT32 length = 0;
    return distribution_of(GetCurrentPackageFullName(&length, nullptr));
}
} // namespace nenenib::adapters::win32
