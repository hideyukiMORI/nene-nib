#pragma once

#include "ContextMenuNeed.hpp"

#include <cstdint>

namespace nenenib::core
{
// 右クリックのメニューに載る操作の置き場（ADR 0078 の追補 #1051 / #1790）。群は区切り線で分かれる
// 塊の番号、順は群の中の並び。どちらも小さい方が上。need は項目が使えるために要るもの。本文の
// メニューは操作の表の列 context_menu、タブのメニューは列 tab_menu に置く。
struct ContextMenuSlot
{
    std::uint8_t group;
    std::uint8_t order;
    ContextMenuNeed need;
};
} // namespace nenenib::core
