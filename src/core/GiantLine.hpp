#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// 巨大な行の定義（ADR 0140 の決定 1・ADR 0139 の決定 1）。表示文字列のコードポイント数が
// これを超える行。しきい値はここ 1 か所。
inline constexpr std::size_t giant_line_threshold = 16'384;
// 窓の中心の刻みと、中心の前後に取るコードポイント数（ADR 0139 の決定 3）。
inline constexpr std::size_t giant_line_step = 2'048;
inline constexpr std::size_t giant_line_half_width = 4'096;

// 巨大な行の窓（ADR 0139 の決定 3）。line は行の内容（行頭から・検証済みの UTF-8）、anchor は
// その中のバイト位置。錨を Vim の 1 文字の先頭へ寄せ、行頭からのコードポイント数を
// giant_line_step 刻みに切り下げた位置を中心に、前後 half_width コードポイントを取り、両端を
// Vim の 1 文字の境目へ外向きに寄せた [begin, end) を行の中のバイト位置で返す（ADR 0053）。
// 錨が刻みをまたがない限り同じ窓になる（窓は状態を持たない）。
[[nodiscard]] OffsetRange giant_line_window(std::string_view line, Offset anchor,
                                            std::size_t half_width) noexcept;
} // namespace nenenib::core
