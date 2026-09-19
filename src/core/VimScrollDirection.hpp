#pragma once

#include <cstdint>

namespace nenenib::core
{
// 画面移動の向き。量の規則（half / page）は別の関数が決める。
enum class VimScrollDirection : std::uint8_t
{
    up,
    down
};
} // namespace nenenib::core
