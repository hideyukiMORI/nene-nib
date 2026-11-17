#pragma once

namespace nenenib::application
{
// メニューの上下の鍵（ADR 0078 の追補 #1051）。使える行だけを回り込んで選ぶ。メニューを閉じない。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct StepContextMenu
{
    bool down;
};
} // namespace nenenib::application
