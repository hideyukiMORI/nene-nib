#pragma once

#include <cstdint>

namespace nenenib::application
{
// 面と本文の境の上のマウス（ADR 0162 の追補・施主決定 D78）。dragged は押したまま動かした（幅を
// 見せるだけで設定に書かない）、released は動かした後に離した（その幅を設定へ書く）、reset は境の
// ダブルクリック（既定の式へ戻して設定へ書く）。閉じた集合（CPP-002）。
enum class ExplorerResizing : std::uint8_t
{
    dragged,
    released,
    reset
};
} // namespace nenenib::application
