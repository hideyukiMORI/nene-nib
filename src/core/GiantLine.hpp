#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// ADR 0139（提案・試作）: 巨大な行は、錨の周りの窓だけを文字組みする。
// しきい値はここ 1 か所。試作は行のバイト数で判定する（コードポイント数は後）。
inline constexpr std::size_t giant_line_threshold = 65'536;
// 錨の前後に取るコードポイント数（窓は最大 2 倍）。
inline constexpr std::size_t giant_line_half_width = 4'096;

// utf8 は行の一部を切り出したバイト列で、両端がコードポイントの途中でもよい（途中の欠片は
// 窓に入れない）。anchor から前後に half コードポイントずつ歩き、両端を Vim の 1 文字の境目へ
// 外向きに寄せた [begin, end) を utf8 の中のバイト位置で返す（ADR 0053）。
[[nodiscard]] OffsetRange giant_line_window(std::string_view utf8, Offset anchor,
                                            std::size_t half) noexcept;
} // namespace nenenib::core
