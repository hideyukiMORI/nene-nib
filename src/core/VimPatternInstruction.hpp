#pragma once

#include "VimPatternOpcode.hpp"

#include <cstddef>
#include <limits>

namespace nenenib::core
{
// open と close の命令の群の番号のうち、番号の無い群（`\%(`）の印。
inline constexpr std::size_t vim_pattern_no_capture = std::numeric_limits<std::size_t>::max();

// 命令の列の 1 命令（ADR 0184 の決定 3）。atom は first が原子の番号、
// split は first と second が行き先（first が先の道）、jump は first が行き先、
// open と close は first が群の番号（番号の無い群は vim_pattern_no_capture）。
// ほかの欄は 0。値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimPatternInstruction
{
    VimPatternOpcode opcode;
    std::size_t first;
    std::size_t second;
};
} // namespace nenenib::core
