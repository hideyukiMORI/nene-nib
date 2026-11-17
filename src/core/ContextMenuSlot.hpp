#pragma once

#include "ContextMenuNeed.hpp"

#include <cstdint>

namespace nenenib::core
{
// 本文の右クリックのメニューに載る操作の置き場（ADR 0078 の追補 #1051）。群は区切り線で分かれる
// 塊の番号、順は群の中の並び。どちらも小さい方が上。need は項目が使えるために要るもの。
struct ContextMenuSlot
{
    std::uint8_t group;
    std::uint8_t order;
    ContextMenuNeed need;
};
} // namespace nenenib::core
