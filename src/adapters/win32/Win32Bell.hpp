#pragma once

#include "BellPort.hpp"

namespace nenenib::adapters::win32
{
// OS の警告音を鳴らす唯一の場所（ARC-007・ADR 0153 の決定 5）。MessageBeep は非同期で、
// 鳴らなくても編集は変わらないので戻り値は見ない。
class Win32Bell final : public application::BellPort
{
  public:
    void ring() noexcept override;
};
} // namespace nenenib::adapters::win32
