#pragma once

#include <cstdint>

namespace nenenib::core
{
// キャレットの形。通常モードと INSERT はバー、Vim の NORMAL はブロック（採用案 第 1 節）。
enum class CaretShape : std::uint8_t
{
    bar,
    block
};
} // namespace nenenib::core
