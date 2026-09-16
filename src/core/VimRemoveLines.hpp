#pragma once

#include "OffsetRange.hpp"

namespace nenenib::core
{
// 行単位の削除（dd・dj・dk）。Vim は削除のあとキャレットをその行の最初の非空白に置くので、
// 文字単位の削除とは別の選択肢にして、写し先を機械に数えさせる（CPP-002 / ADR 0012 の決定 3）。
struct VimRemoveLines
{
    OffsetRange range;
};
} // namespace nenenib::core
