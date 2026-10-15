#include "ExperimentLog.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <format>
#include <string>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] std::int64_t ticks_of(FILETIME time) noexcept
{
    return (static_cast<std::int64_t>(time.dwHighDateTime) << 32U) |
           static_cast<std::int64_t>(time.dwLowDateTime);
}

[[nodiscard]] double milliseconds_since_creation() noexcept
{
    FILETIME creation{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exited, &kernel, &user) == FALSE)
    {
        return -1.0;
    }
    FILETIME now{};
    GetSystemTimePreciseAsFileTime(&now);
    constexpr double ticks_per_millisecond = 10000.0;
    return static_cast<double>(ticks_of(now) - ticks_of(creation)) / ticks_per_millisecond;
}
} // namespace

bool experiment_flag(const wchar_t *name)
{
    std::array<wchar_t, 8> value{};
    const DWORD length =
        GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size()));
    return length > 0;
}

void experiment_note(std::string_view text)
{
    const std::string line =
        std::format("[nib-warp] {:.2f} ms {}\n", milliseconds_since_creation(), text);
    OutputDebugStringA(line.c_str());
    std::array<wchar_t, MAX_PATH> path{};
    const DWORD length =
        GetEnvironmentVariableW(L"NIB_EXPERIMENT_LOG", path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
    {
        return;
    }
    HANDLE file = CreateFileW(path.data(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    static_cast<void>(
        WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr));
    CloseHandle(file);
}
} // namespace nenenib::adapters::win32
