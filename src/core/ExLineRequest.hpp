#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// Ex の行番号だけの命令 `:N` `:$`（ADR 0022 の追補・Issue #847）。line は書いたままの N（`:0` は
// 0・先頭の 0 は読み飛ばす・大きすぎる数は size_t の最大に飽和）で、`:$` は空（最後の行）。
// 範囲の外かどうかは本文を知っている engine が `{N}G` と同じ着地で決める（`:0` は 1 行目・行数を
// 超えたら最後の行）。
struct ExLineRequest
{
    std::optional<std::size_t> line;
};
} // namespace nenenib::core
