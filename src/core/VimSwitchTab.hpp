#pragma once

#include <cstddef>

namespace nenenib::core
{
// `gt` `gT` が決めた行き先のタブへ切り替える（ADR 0057 の決定 2）。index は帯の位置
// （0 始まり）。controller は窓の SwitchTab と同じ経路へ写し、今いるタブなら何もしない。
struct VimSwitchTab
{
    std::size_t index;
};
} // namespace nenenib::core
