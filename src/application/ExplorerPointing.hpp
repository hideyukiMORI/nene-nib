#pragma once

#include <cstdint>

namespace nenenib::application
{
// 面の行の上のマウス（ADR 0162 の決定 4・ADR 0056 の決定 9）。pressed は押した、released は
// 離した、double はダブルクリック。閉じた集合（CPP-002）。
enum class ExplorerPointing : std::uint8_t
{
    pressed,
    released,
    double_click
};
} // namespace nenenib::application
