#include "Win32MotionClock.hpp"

#include <windows.h>

#include <cstdint>

namespace nenenib::adapters::win32
{
Win32MotionClock::Win32MotionClock() noexcept
{
    LARGE_INTEGER frequency{};
    if (QueryPerformanceFrequency(&frequency) != 0 && frequency.QuadPart > 0)
    {
        frequency_ = frequency.QuadPart;
    }
}

std::uint64_t Win32MotionClock::milliseconds() noexcept
{
    constexpr std::int64_t milliseconds_per_second = 1000;
    LARGE_INTEGER counter{};
    if (frequency_ <= 0 || QueryPerformanceCounter(&counter) == 0 || counter.QuadPart < 0)
    {
        return 0;
    }
    // 秒と端数に分けて掛ける（counter × 1000 の桁あふれを避ける）。
    const std::int64_t seconds = counter.QuadPart / frequency_;
    const std::int64_t rest = counter.QuadPart % frequency_;
    return static_cast<std::uint64_t>((seconds * milliseconds_per_second) +
                                      ((rest * milliseconds_per_second) / frequency_));
}
} // namespace nenenib::adapters::win32
