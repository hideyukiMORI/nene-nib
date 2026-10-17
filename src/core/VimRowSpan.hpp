#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"

#include <cstddef>

namespace nenenib::core
{
// 行 line の row 番目（0 始まり）の画面行が占める本文の範囲 [start, end) と、行の画面行の総数
// （ADR 0144 の決定 2）。最後の画面行の end は行の内容の終わり。start_column は start の行頭からの
// 仮想桁（0 始まり・索引から O(塊)・決定 3 の追補）。1 打鍵の間だけ借りる答え。
struct VimRowSpan
{
    LineNumber line;
    std::size_t row;
    Offset start;
    Offset end;
    std::size_t rows;
    std::size_t start_column;
};
} // namespace nenenib::core
