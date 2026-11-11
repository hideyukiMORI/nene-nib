#pragma once

#include "ExplorerResizing.hpp"

#include <cstdint>

namespace nenenib::application
{
// 面の幅を境のドラッグで変える（ADR 0162 の追補・施主決定 D78）。使う人の操作。width_dips は
// core::explorer_dragged_width が範囲に切り詰めた幅（DIP）で、窓が押した点からのずれを引いて
// 求める。dragged の間の幅は application の状態だけが持ち（面と本文の配置が引く）、released で
// 設定 explorer_width へ 1 回書く。動かさずに離した released は何もしない。reset では width_dips
// を読まない（設定を「無し」＝既定の式にする）。cancelled も width_dips を読まず、dragged の幅を
// 捨てて押す前の幅（設定の値）へ戻し、設定に書かない（ドラッグ中の Esc・Issue #1413）。
struct ResizeExplorer
{
    ExplorerResizing resizing;
    std::int32_t width_dips;
};
} // namespace nenenib::application
