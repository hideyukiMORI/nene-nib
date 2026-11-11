#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// N 回の編集の後の 1 鍵（#1473・ADR 0180）。712 字の 1 行目の行頭で Vim の `rb` `rc` を交互に N 回
// 流して undo の単位を N 個積んでから（区間の外・行の長さは変わらない）、編集しない鍵 `l` `h` を
// 100 組か、編集する鍵 `x` を 200 回流す（鍵ごとに frame
// を読む）。状態の写しが履歴を複製しなければ、 どちらも N に比例しない。
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
