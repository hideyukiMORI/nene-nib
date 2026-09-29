#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ctrl+P の面の候補の出どころ（ADR 0057 の決定 7）。commands は Ex の命令の候補、tabs は開いて
// いるタブの一覧（「∨」・`:tabs`・候補の `tabs`）。値が増えたら候補を作る switch が落ちる
// （CPP-002）。
enum class CommandPaletteSource : std::uint8_t
{
    commands,
    tabs
};
} // namespace nenenib::core
