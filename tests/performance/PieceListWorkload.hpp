#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// piece の数 P が多い本文の費用（#1474・ADR 0183 の決定 5）。区間の外で、20,000 行 × 80 B の本文の
// 等間隔の行頭へ `#` を P / 2 回入れて P を作る（1 回で piece が 2 つ増える・P = 1 は編集しない）。
// read は真ん中の 40 行で読む口（line_start / line_end / line_text / position_of / offset_of と
// 64 B の text_range）を 50 周、replace は列を伸ばさない replaced を 200 回（元の値から毎回）、
// replace_all は 10 万か所の substituted_all を 1 回（本文は 10 万行・P は 1 か約 8,000）、frame は
// controller で Vim の `I#<Esc>j` を P / 2 周してから `l` `h` を 100 組（鍵ごとに frame を読む）。
// 記録だけでゲートの必須にしない（ADR 0138 と同じ扱い）。
enum class PieceListWorkload : std::uint8_t
{
    reads_after_1,
    reads_after_1000,
    reads_after_4000,
    reads_after_16000,
    replaces_after_1,
    replaces_after_1000,
    replaces_after_4000,
    replaces_after_16000,
    replace_all_after_1,
    replace_all_after_8000,
    frame_after_1,
    frame_after_16000
};
} // namespace nenenib::tests::performance
