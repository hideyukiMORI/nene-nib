#pragma once

#include "LayoutRect.hpp"

#include <cstddef>

namespace nenenib::core
{
// 検索欄の記号の案内の置き場（#1640・ADR 0060 の決定 9 の追補）。area は案内の右端を検索欄の右端に
// 揃えた欄で、幅は出す文の幅。first は出す最初の組（記号 ＋ 名前）の番号で、first から最後の組
// までを出す。1 つも入らなければ first は組の数で、area は検索欄の右端の幅 0。
struct PaletteHintPlace
{
    LayoutRect area;
    std::size_t first;
};
} // namespace nenenib::core
