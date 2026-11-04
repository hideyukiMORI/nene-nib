#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 巨大な 1 行の矩形 `I` の入口（#1198）。16 MiB の 1 行（索引を持つ本文・ADR 0140 の決定 4）で
// `$` の後の `<C-v>`（鍵と frame が引く矩形）と `I` の鍵を 1 回ずつ流す。残りの 3 本は同じ本文の
// 真ん中で、矩形の `r`・矩形のレジスタの `p`・`I` の後の `.` の再生（#1237）。
enum class VimBlockWorkload : std::uint8_t
{
    insert_giant_1line,
    replace_giant_1line,
    put_giant_1line,
    repeat_giant_1line
};
} // namespace nenenib::tests::performance
