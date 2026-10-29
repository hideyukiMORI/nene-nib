#pragma once

#include "VimModeSet.hpp"

#include <string>

namespace nenenib::core
{
// 設定 v4 の `map.<modes>.<lhs>=<rhs>` の 1 行（ADR 0176 の決定 1）。左辺と右辺は鍵の記法の
// 文字列のまま持ち（`<leader>` も展開しない）、表 KeyMap は key_map_of が作る。remap が偽なら
// `noremap`（値の先頭の `!`）。値は単独で妥当な公開 aggregate で、比較は非メンバー（CPP-003）。
struct MapRow
{
    VimModeSet modes;
    std::string lhs;
    std::string rhs;
    bool remap = true;
};

[[nodiscard]] inline bool operator==(const MapRow &left, const MapRow &right) noexcept
{
    return left.modes == right.modes && left.lhs == right.lhs && left.rhs == right.rhs &&
           left.remap == right.remap;
}
} // namespace nenenib::core
