#pragma once

#include "ClockPort.hpp"
#include "LocalDateTime.hpp"

namespace nenenib::adapters::win32
{
// 現在時刻（地方時）を読む唯一の場所（ARC-007・ADR 0175 の決定 1）。GetLocalTime の SYSTEMTIME を
// 写すだけで、形は core の date_time_text が決める（決定 2）。
class Win32Clock final : public application::ClockPort
{
  public:
    [[nodiscard]] core::LocalDateTime now() override;
};
} // namespace nenenib::adapters::win32
