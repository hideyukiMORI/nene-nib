#pragma once

#include <cstdint>

namespace nenenib::core
{
// Vim の `Ctrl-w` の 2 打鍵目が名指す窓の命令の閉じた一覧（ADR 0148 の決定 7）。engine は枠の数を
// 知らないので、この値を効果として返すだけで、「既に 2 枠」などの判断は controller がする。
// 2 打鍵目の鍵 → 命令の表は VimKeyTable.cpp の 1 つ（ADR 0042 の決定 6）。
enum class VimWindowCommand : std::uint8_t
{
    split_horizontal,
    split_vertical,
    next,
    left,
    down,
    up,
    right,
    close,
    only
};
} // namespace nenenib::core
