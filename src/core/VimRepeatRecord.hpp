#pragma once

#include "VimCount.hpp"
#include "VimKey.hpp"
#include "VimVisualExtent.hpp"

#include <optional>
#include <vector>

namespace nenenib::core
{
// 記録中の鍵の列と、直前の変更（ADR 0030 の決定 1）。回数は完了時の積を 1 つだけ持ち、
// 鍵の列に回数の桁は入らない。本文・履歴・レジスタは持たない（ARC-004）。
// visual は VISUAL で行った変更の「範囲の大きさ」（ADR 0033 の決定 1）。空は NORMAL の記録で、
// あれば `.` がキャレットの所に同じ大きさを選び直してから鍵を流す。
struct VimRepeatRecord
{
    std::optional<VimCount> count;
    std::vector<VimKey> keys;
    std::optional<VimVisualExtent> visual;
};
} // namespace nenenib::core
