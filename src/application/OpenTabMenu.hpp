#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// タブの右クリックのメニューを開く（ADR 0078 の追補 #1790・施主決定 D98）。x と y は開く点
// （クライアント座標の物理画素・ui が帯のすぐ下に置く）、tab は右クリックしたタブの帯の位置（0
// 始まり）。controller は開いたときにタブを退避番号（autosave_ids）で覚え、帯の位置では名指し
// し続けない。アクティブでないタブでもよく、切り替えない。使う人の操作なので keeps_tab_walk にも
// begin_intent の表にも書かない。
struct OpenTabMenu
{
    std::int32_t x;
    std::int32_t y;
    std::size_t tab;
};
} // namespace nenenib::application
