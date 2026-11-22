#pragma once

#include <cstdint>

namespace nenenib::application
{
// キャレットの動きの経過を測る時計（ADR 0192 の決定 3）。窓は動いている間だけここを読み、経過を
// 合図 CaretMotionTick に載せる。値は単調に増える ms で、起点に意味は無い（差だけを使う）。
// 実装は src/adapters/win32 だけが持つ（ARC-003 / ARC-007）。試験は時計を読まない。
class MotionClockPort
{
  public:
    MotionClockPort() = default;
    virtual ~MotionClockPort() = default;
    MotionClockPort(const MotionClockPort &) = delete;
    MotionClockPort(MotionClockPort &&) = delete;
    MotionClockPort &operator=(const MotionClockPort &) = delete;
    MotionClockPort &operator=(MotionClockPort &&) = delete;

    [[nodiscard]] virtual std::uint64_t milliseconds() noexcept = 0;
};
} // namespace nenenib::application
