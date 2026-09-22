#pragma once

#include "Offset.hpp"

namespace nenenib::core
{
// 検索が着いた場所（ADR 0032 の決定 3）。wrapped は本文の端を越えて折り返したかで、
// 越えたときだけ「search hit BOTTOM, continuing at TOP」の報せを出す（wrapscan は既定で有効）。
struct VimSearchHit
{
    Offset caret;
    bool wrapped;
};
} // namespace nenenib::core
