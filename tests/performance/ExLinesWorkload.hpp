#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// `:d` と `:g` の費用（ADR 0168 の決定 9・段 3a・#1654）。10 万行（`lineNNNNNN x` と `lineNNNNNN y`
// が交互・約 1.3 MiB）の文書で `:g/x/d`（当たりは半分の 5 万行）と `:%d` を engine の 1 歩
// （vim_global・vim_lines_edited）で流し、行の塊の編集を本文に当てるまで。行ごとに本文全体を写す
// 2 乗の形になっていないかを見る。
enum class ExLinesWorkload : std::uint8_t
{
    global_delete_half_100k,
    delete_all_100k
};
} // namespace nenenib::tests::performance
