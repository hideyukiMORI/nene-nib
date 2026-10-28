#pragma once

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// 数を探す 1 行の窓（ADR 0015 の決定 14）。`bytes` は行の一部か全部（改行は含まない）で、
// `at` は窓の中の探し始め（NORMAL はキャレット・VISUAL はその行の選択の始まり）のバイト位置。
// `from_line_start` / `to_line_end` は窓の端が行の端か。行の端でない端まで読みに行くと
// VimNumberMiss::window_short を返すので、巨大な行でも行の全体を写さずに済む。
struct VimNumberWindow
{
    std::string_view bytes;
    std::size_t at;
    bool from_line_start;
    bool to_line_end;
};
} // namespace nenenib::core
