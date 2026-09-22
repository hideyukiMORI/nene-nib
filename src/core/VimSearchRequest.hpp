#pragma once

#include "Offset.hpp"
#include "VimSearchDirection.hpp"

namespace nenenib::core
{
// 1 つの検索の依頼（ADR 0032 の決定 3）。origin は探し始める位置、anchor はオペレータと
// VISUAL が範囲の端に使う位置で、`*` / `#` だけがこの 2 つが違う（語の先頭から探すが、
// 範囲の端は元のキャレット。`G$` のあとの `d#` が 2 文字だけ消すのはこれ・実測）。
// direction はこの 1 回に効く向き（`N` は覚えている向きの反対）。
struct VimSearchRequest
{
    Offset origin;
    Offset anchor;
    VimSearchDirection direction;
};
} // namespace nenenib::core
