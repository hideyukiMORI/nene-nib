#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <cstddef>

namespace nenenib::core
{
// NORMAL のキャレットは文字の上にある（行末を越えない・空行だけ行頭）。効果を当てたあとの
// 位置をここで 1 か所に寄せる（ADR 0012 の決定 5）。
[[nodiscard]] Offset vim_resting_caret(const TextBuffer &text, Offset caret);

// at の行の最初の非空白。行が空白だけなら行の最後の文字（空行は行頭）。行単位の削除の
// あとの位置（Vim の規則）。
[[nodiscard]] Offset vim_first_non_blank(const TextBuffer &text, Offset at);

// at から右へ空白（空白と Tab）を越えた最初の文字。at の行の中だけを歩き、空白しか残らなければ
// 行の最後の文字（Vim の ^ と g^ の oneright の歩き・ADR 0147 の決定 8）。at が行の内容の終わり
// ならそのまま。vim_first_non_blank は行頭から歩くこの 1 本。
[[nodiscard]] Offset vim_non_blank_from(const TextBuffer &text, Offset at);

// line の最後の非空白の文字の先頭。空白だけの行と空行は行頭（Vim の g_・ADR 0147 の決定 8）。
[[nodiscard]] Offset vim_last_non_blank(const TextBuffer &text, LineNumber line);

// line の上のバイトの桁 column の位置。桁が行の内容を越えたら行の内容の終わり（NORMAL の寄せは
// vim_resting_caret）、文字の途中ならその文字の先頭。行の桁の写しはこの 1 本（ADR 0052 の決定
// 5・9）。
[[nodiscard]] Offset vim_line_and_column(const TextBuffer &text, LineNumber line,
                                         std::size_t column);

// before の上の at の行とバイトの桁を、after の同じ行と桁へ写す（Vim の Ctrl-r の戻り先・ADR 0052
// の決定 5）。桁が行の内容を越えたら行の内容の終わり（NORMAL の寄せは vim_resting_caret）、文字の
// 途中ならその文字の先頭、行が after に無ければ最後の行の最初の非空白。
[[nodiscard]] Offset vim_same_line_and_column(const TextBuffer &before, Offset at,
                                              const TextBuffer &after);
} // namespace nenenib::core
