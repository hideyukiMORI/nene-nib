#pragma once

#include "TextPosition.hpp"

namespace nenenib::core
{
// 検索が着いた場所（ADR 0032 の決定 3・ADR 0041 の決定 1）。wrapped は本文の端を越えて
// 折り返したかで、越えたときだけ「search hit BOTTOM, continuing at TOP」の報せを出す
// （wrapscan は既定で有効）。確定の検索と incsearch の preview が同じ値を受け取る。
struct VimSearchHit
{
    TextPosition position;
    bool wrapped;
};
} // namespace nenenib::core
