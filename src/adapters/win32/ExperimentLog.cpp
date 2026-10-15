#include "ExperimentLog.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

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

[[nodiscard]] std::vector<std::pair<std::uint32_t, std::int64_t>> &stamps()
{
    static std::vector<std::pair<std::uint32_t, std::int64_t>> recorded = [] {
        std::vector<std::pair<std::uint32_t, std::int64_t>> reserved;
        constexpr std::size_t capacity = 4096;
        reserved.reserve(capacity);
        return reserved;
    }();
    return recorded;
}

void append(const std::string &line)
{
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
} // namespace

void experiment_stamp(std::uint32_t point)
{
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    stamps().emplace_back(point, counter.QuadPart);
}

void experiment_flush()
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    std::string lines;
    for (const auto &[point, counter] : stamps())
    {
        lines += std::format("stamp {} {} {}\n", point, counter, frequency.QuadPart);
    }
    if (!lines.empty())
    {
        append(lines);
    }
}

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
    append(line);
}
} // namespace nenenib::adapters::win32
