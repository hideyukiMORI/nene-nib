#pragma once

#include "SyntaxRole.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::core
{
// ミニマップの 1 行の中の 1 つの塊（ADR 0171 の決定 3）。left は行頭からの桁（0 始まり・1 桁 =
// 1 物理画素）、width は桁の数。role は構文の色の区間の役割で、区間の無い文字は値なし（muted で
// 描く）。空白は塊にならない。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct MinimapBlock
{
    std::uint16_t left;
    std::uint16_t width;
    std::optional<SyntaxRole> role;
};

[[nodiscard]] constexpr bool operator==(const MinimapBlock &left,
                                        const MinimapBlock &right) noexcept
{
    return left.left == right.left && left.width == right.width && left.role == right.role;
}

// 1 行の塊の列（左から順・重ならない）。空の行・空白だけの行は空。
using MinimapRow = std::vector<MinimapBlock>;
} // namespace nenenib::core
