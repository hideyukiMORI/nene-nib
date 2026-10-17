#pragma once

#include "LineNumber.hpp"
#include "Selection.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 画面移動の結果。NORMAL の畳んだ選択または VISUAL の anchor を保った選択と、画面の
// 先頭行を一緒に返し、application が同じ遷移で反映する（ADR 0019 の決定 3）。first_row は
// 先頭行の中の画面行で、画面行の単位の移動だけが 0 以外を置く（ADR 0144 の決定 6）。first_column は
// 横送りの鍵が決めた新しい leftcol（升・0 始まり）で、縦の移動は無し（横位置を保って追従する・
// ADR 0145 の決定 7・8）。
struct VimNavigate
{
    Selection selection;
    LineNumber first_visible;
    std::size_t first_row = 0;
    std::optional<std::size_t> first_column = std::nullopt;
};
} // namespace nenenib::core
