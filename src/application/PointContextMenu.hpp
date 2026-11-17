#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// メニューの上のマウスが載せた行（ADR 0078 の追補 #1051）。無しは行の外（区切り線・空き・面の外）。
// 使えない行は選ばない。メニューを閉じない。使う人の操作なので keeps_tab_walk にも begin_intent の
// 表にも書かない（メニューが開いている間は歩きも知らせも無い）。
struct PointContextMenu
{
    std::optional<std::size_t> row;
};
} // namespace nenenib::application
