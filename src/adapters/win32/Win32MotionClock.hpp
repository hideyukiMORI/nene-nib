#pragma once

#include "MotionClockPort.hpp"

#include <cstdint>

namespace nenenib::adapters::win32
{
// キャレットの動きの時計（ADR 0192 の決定 3）。QueryPerformanceCounter を ms に写す。読めなければ
// 0 を返し、動きは進まずに次の合図で跳ぶ（描く側は経過が時間に届くまで動かすだけ）。
class Win32MotionClock final : public application::MotionClockPort
{
  public:
    Win32MotionClock() noexcept;
    ~Win32MotionClock() override = default;
    Win32MotionClock(const Win32MotionClock &) = delete;
    Win32MotionClock(Win32MotionClock &&) = delete;
    Win32MotionClock &operator=(const Win32MotionClock &) = delete;
    Win32MotionClock &operator=(Win32MotionClock &&) = delete;

    [[nodiscard]] std::uint64_t milliseconds() noexcept override;

  private:
    std::int64_t frequency_ = 0;
};
} // namespace nenenib::adapters::win32
