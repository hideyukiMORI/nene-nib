#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// `gn` `gN` の費用（#1617 のレビュー M4）。1 MiB の 1 行（`ab ` の繰り返しの後ろに `ab`・改行なし）
// で最後の検索 `/ab` を置き、engine の 1 歩（vim_step）に鍵を流す。比べる相手は同じ行・同じ
// キャレットの `n` `N`。キャレットは行頭（at_start）か最後の文字（at_end）。
enum class SearchObjectWorkload : std::uint8_t
{
    n_at_start,
    gn_at_start,
    dgn_at_start,
    n_at_end,
    previous_at_end,
    gn_at_end,
    dgn_at_end,
    dgprevious_at_end
};
} // namespace nenenib::tests::performance
