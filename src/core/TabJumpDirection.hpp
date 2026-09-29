#pragma once

#include <cstdint>

namespace nenenib::core
{
// タブの行き先の向き（ADR 0057 の決定 1）。forward は `gt` と `:tabnext`、backward は `gT` と
// `:tabprevious` `:tabNext`。値が増えたら tab_destination の switch が落ちる（CPP-002）。
enum class TabJumpDirection : std::uint8_t
{
    forward,
    backward
};
} // namespace nenenib::core
