#pragma once

#include "OffsetRange.hpp"

namespace nenenib::core
{
// 文字単位の削除（x・d＋文字の移動・INSERT の Backspace）。キャレットは範囲の先頭に残る。
struct VimRemoveRange
{
    OffsetRange range;
};
} // namespace nenenib::core
