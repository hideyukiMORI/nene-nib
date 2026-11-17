#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// メニューの行を実行する（Enter は選んでいる行・クリックは row・ADR 0078 の追補 #1051）。使える行
// なら閉じてから、Vim の NORMAL と VISUAL は表の Vim の鍵の列を engine に流し、ほかは
// operation_request を立てて ui の run_operation の 1 本に渡す。使えない行と行の外は何もしない
// （開いたまま）。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ChooseContextMenu
{
    std::optional<std::size_t> row;
};
} // namespace nenenib::application
