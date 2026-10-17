#pragma once

#include <cstdint>

namespace nenenib::core
{
// 画面移動（PgDn / PgUp / Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u）の単位（ADR 0144 の決定 5）。line は
// Vim 9.1 の論理行の巻き（ADR 0019）、row は画面行の巻き（格子）。
enum class VimScrollUnit : std::uint8_t
{
    line,
    row
};
} // namespace nenenib::core
