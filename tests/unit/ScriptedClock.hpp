#pragma once

#include "ClockPort.hpp"
#include "LocalDateTime.hpp"

#include <cstddef>

namespace nenenib::tests
{
// 現在時刻の替え玉。OS の時刻を読まずに試験が置いた値を返し、呼ばれた回数を数える（ADR 0175 の
// 決定 1・4）。試験が実時刻を読むことも決定性の破壊なので、試験はこれだけを使う。
class ScriptedClock final : public nenenib::application::ClockPort
{
  public:
    [[nodiscard]] nenenib::core::LocalDateTime now() override
    {
        ++calls_;
        return value_;
    }

    void set(nenenib::core::LocalDateTime value) noexcept
    {
        value_ = value;
    }

    [[nodiscard]] std::size_t calls() const noexcept
    {
        return calls_;
    }

  private:
    nenenib::core::LocalDateTime value_{2026, 10, 29, 12, 34, 56};
    std::size_t calls_ = 0;
};
} // namespace nenenib::tests
