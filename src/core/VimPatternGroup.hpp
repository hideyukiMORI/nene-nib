#pragma once

#include <cstddef>
#include <limits>

namespace nenenib::core
{
// 解析の途中で閉じていない群の end。
inline constexpr std::size_t vim_pattern_group_open = std::numeric_limits<std::size_t>::max();

// パターンの群 `\(` `\)` の 1 つ（ADR 0168 の決定 4）。原子の列の [first, end) を囲む。群は照合の
// 答え（一致の範囲）を変えない（群に量指定子を付けられないので、群を外したパターンと同じ所に
// 当たる）ので原子の列には入らず、置換が群の範囲を引くとき（VimPattern::captured）だけ読む。
// 値は単独で妥当なので公開 aggregate（CPP-003）。
struct VimPatternGroup
{
    std::size_t first;
    std::size_t end;
};
} // namespace nenenib::core
