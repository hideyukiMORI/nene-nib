#include "Win32Clock.hpp"

#include <windows.h>

#include <cstdint>

namespace nenenib::adapters::win32
{
core::LocalDateTime Win32Clock::now()
{
    SYSTEMTIME local{};
    // GetLocalTime は失敗しない（戻り値なし）。
    GetLocalTime(&local);
    return core::LocalDateTime{local.wYear,
                               static_cast<std::uint8_t>(local.wMonth),
                               static_cast<std::uint8_t>(local.wDay),
                               static_cast<std::uint8_t>(local.wHour),
                               static_cast<std::uint8_t>(local.wMinute),
                               static_cast<std::uint8_t>(local.wSecond)};
}
} // namespace nenenib::adapters::win32
