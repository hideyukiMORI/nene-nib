#pragma once

#include "Offset.hpp"
#include "TextBuffer.hpp"

namespace nenenib::core
{
// NORMAL のキャレットは文字の上にある（行末を越えない・空行だけ行頭）。効果を当てたあとの
// 位置をここで 1 か所に寄せる（ADR 0012 の決定 5）。
[[nodiscard]] Offset vim_resting_caret(const TextBuffer &text, Offset caret);

// at の行の最初の非空白。行が空白だけなら行頭。行単位の削除のあとの位置（Vim の規則）。
[[nodiscard]] Offset vim_first_non_blank(const TextBuffer &text, Offset at);
} // namespace nenenib::core
