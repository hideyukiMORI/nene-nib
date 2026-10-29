#pragma once

#include <cstdint>

namespace nenenib::core
{
// 矩形の INSERT が 1 行目に打った本文を残りの行のどこへ写すか（ADR 0035 の追補 #1042）。
// `before` は `I` `c` `C`（矩形の左の桁・そこまで届かない行は飛ばす）、`after` は `A`（右の桁の
// 次・届かない行は空白で埋める）、`line_end` は `$` の `A`（各行の内容の終わり・埋めない）。
enum class VimBlockInsertSide : std::uint8_t
{
    before,
    after,
    line_end
};
} // namespace nenenib::core
