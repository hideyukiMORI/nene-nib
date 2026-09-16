#pragma once

#include <cstdint>

namespace nenenib::core
{
// Vim のモードの閉じた一覧（ADR 0012 の決定 1）。VISUAL とコマンドラインは次の縦切り。
enum class VimMode : std::uint8_t
{
    normal,
    insert
};
} // namespace nenenib::core
