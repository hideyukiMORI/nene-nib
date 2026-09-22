#pragma once

#include "VimSearchDirection.hpp"

namespace nenenib::core
{
// 検索の入力行を開く効果（ADR 0032 の決定 2）。VimState は変えないので、保留中のオペレータ・
// 回数・記録中の鍵は入力のあいだそのまま保たれる。
struct VimOpenSearch
{
    VimSearchDirection direction;
};
} // namespace nenenib::core
