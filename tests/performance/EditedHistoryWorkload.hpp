#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// N 回の編集の後の 1 鍵（#1473・ADR 0180）。1 行目の行頭で Vim の `x` を N 回流して undo の単位を
// N 個積んでから（区間の外）、編集しない鍵 `l` `h` を 100 組か、編集する鍵 `x` を 200 回流す。
// 状態の写しが履歴を複製しなければ、どちらも N に比例しない。行の長さは N に依らない（行頭の鍵）。
enum class EditedHistoryWorkload : std::uint8_t
{
    moves_after_1000,
    moves_after_4000,
    moves_after_16000,
    erases_after_1000,
    erases_after_4000,
    erases_after_16000
};
} // namespace nenenib::tests::performance
