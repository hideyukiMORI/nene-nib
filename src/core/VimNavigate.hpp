#pragma once

#include "LineNumber.hpp"
#include "Selection.hpp"

namespace nenenib::core
{
// 画面移動の結果。NORMAL の畳んだ選択または VISUAL の anchor を保った選択と、画面の
// 先頭行を一緒に返し、application が同じ遷移で反映する（ADR 0019 の決定 3）。
struct VimNavigate
{
    Selection selection;
    LineNumber first_visible;
};
} // namespace nenenib::core
