#pragma once

#include "TimingPort.hpp"
#include "WslDistributionPort.hpp"

namespace nenenib::ui::win32
{
// 窓が controller を通さずに借りる合成ルートの port。速さの節目（ADR 0011 の決定 1）と、フォルダ
// 選びの箱に足す WSL のディストリ（ADR 0162 の追補 #1450・箱を出すときだけ読む）。
struct WindowPorts final
{
    application::TimingPort &timing;
    application::WslDistributionPort &wsl;
};
} // namespace nenenib::ui::win32
