#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <cstddef>

namespace nenenib::core
{
class TextBuffer;
class VirtualColumnIndex;

// 巨大な行の定義（ADR 0140 の決定 1・ADR 0139 の決定 1）。表示文字列のコードポイント数が
// これを超える行。しきい値はここ 1 か所。
inline constexpr std::size_t giant_line_threshold = 16'384;
// 窓の中心の刻みと、中心の前後に取るコードポイント数（ADR 0139 の決定 3）。窓の大きさを決める値は
// この 2 つだけで、呼ぶ側は変えない。前後が刻み以上なので、窓は必ず錨を含む。
inline constexpr std::size_t giant_line_step = 2'048;
inline constexpr std::size_t giant_line_half_width = 4'096;
static_assert(giant_line_half_width >= giant_line_step, "the window must contain its anchor");

// 巨大な行の窓（ADR 0139 の決定 3）。line は text の行、anchor は本文のバイト位置（行の外なら
// 行の中へ丸める）。錨を Vim の 1 文字の先頭へ寄せ、行頭からのコードポイントの序数を
// giant_line_step 刻みに切り下げた位置を中心に、前後 giant_line_half_width コードポイントを取り、
// 両端を Vim の 1 文字の境目へ外向きに寄せた [begin, end) を本文のバイト位置で返す（ADR 0053）。
// 序数とその逆引きは index（text の仮想桁の部分和）から O(塊) で引き、行の全体を写さない。
// 本文を読むのは錨と両端の近くだけ。錨が刻みをまたがない限り同じ窓になる（窓は状態を持たない）。
[[nodiscard]] OffsetRange giant_line_window(const VirtualColumnIndex &index, const TextBuffer &text,
                                            LineNumber line, Offset anchor);
} // namespace nenenib::core
