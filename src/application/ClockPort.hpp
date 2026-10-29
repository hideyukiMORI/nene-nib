#pragma once

#include "LocalDateTime.hpp"

namespace nenenib::application
{
// 現在時刻の唯一の入口（ADR 0175 の決定 1）。時刻を読むのは実装（src/adapters/win32）だけで
// （ARC-007）、controller が呼ぶのは F5 で日時を挿入する意図 InsertDateTime の 1 か所だけ（決定
// 4）。
class ClockPort
{
  public:
    ClockPort() = default;
    virtual ~ClockPort() = default;
    ClockPort(const ClockPort &) = delete;
    ClockPort(ClockPort &&) = delete;
    ClockPort &operator=(const ClockPort &) = delete;
    ClockPort &operator=(ClockPort &&) = delete;

    [[nodiscard]] virtual core::LocalDateTime now() = 0;
};
} // namespace nenenib::application
