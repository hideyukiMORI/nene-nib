#pragma once

#include "Column.hpp"

#include <cstddef>

namespace nenenib::application
{
// 表示文字列の 1 つの画面行（ADR 0144 の決定 2・#664）。start と end は表示の桁（1 始まりの挿入点）
// で [start, end)。end は次の画面行の start で、最後の画面行の end は表示文字列の末尾の次の桁。
// 段の末尾にぶら下がる空白はその段に入る。row は 0 始まり、rows は表示文字列の画面行の総数。
// 文字組みの境界（TextLayoutPort::row）が同じ 1 つの layout の行の metrics から作る値。
struct TextRow
{
    std::size_t row;
    core::Column start;
    core::Column end;
    std::size_t rows;
};
} // namespace nenenib::application
