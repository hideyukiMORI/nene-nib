#pragma once

#include "VimCount.hpp"
#include "VimKey.hpp"

#include <optional>
#include <vector>

namespace nenenib::core
{
// 記録中の鍵の列と、直前の変更（ADR 0030 の決定 1）。回数は完了時の積を 1 つだけ持ち、
// 鍵の列に回数の桁は入らない。本文・履歴・レジスタは持たない（ARC-004）。
struct VimRepeatRecord
{
    std::optional<VimCount> count;
    std::vector<VimKey> keys;
};
} // namespace nenenib::core
