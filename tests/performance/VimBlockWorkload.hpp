#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 巨大な 1 行の矩形 `I` の入口（#1198）。16 MiB の 1 行（索引を持つ本文・ADR 0140 の決定 4）で
// `$` の後の `<C-v>`（鍵と frame が引く矩形）と `I` の鍵を 1 回ずつ流す。残りの 3 本は同じ本文の
// 真ん中で、矩形の `r`・矩形のレジスタの `p`・`I` の後の `.` の再生（#1237）。最後の 2 本も同じ
// 本文の真ん中で、INSERT の `<C-w>` と大きな回数の `h`（#1258）と大きな回数の `l`（#1269）。
// 文の移動の 1 本は `text-object-sentence-giant-1line-16384` と同じ巨大な行のしきい値ちょうどの
// 1 行（`.` `!` `?` を含まない）の真ん中から `)` の鍵を 1 回流す（#1316・#751 の塊の走査）。
// 通常モードの語の境の 1 本は、矩形の 1 本と同じ 16 MiB の 1 行の真ん中で、Ctrl+Backspace と
// Ctrl+Delete が消す範囲の端（ordinary_word_boundary の後ろ向きと前向き）を 1 回ずつ引く（#1394）。
// 最後の 2 本は 120 行の文書の末尾で INSERT の `<CR>` と 1 文字を 100 組打ち、窓を 1 行ずつ送る
// （折り返す窓と折り返さない窓・鍵ごとに frame を読む・#1300 の追従の確かめの費用）。
enum class VimBlockWorkload : std::uint8_t
{
    insert_giant_1line,
    replace_giant_1line,
    put_giant_1line,
    repeat_giant_1line,
    insert_ctrl_w_giant_1line,
    count_h_giant_1line,
    count_l_giant_1line,
    sentence_motion_giant_1line,
    ordinary_word_giant_1line,
    insert_scroll_wrapped,
    insert_scroll_unwrapped
};
} // namespace nenenib::tests::performance
