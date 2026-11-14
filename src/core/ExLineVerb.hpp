#pragma once

#include <cstdint>

namespace nenenib::core
{
// 行を消すか写すかの Ex の命令（ADR 0168 の決定 1・段 3a）。`:d[elete]` と `:y[ank]`。
enum class ExLineVerb : std::uint8_t
{
    delete_lines,
    yank_lines
};
} // namespace nenenib::core
