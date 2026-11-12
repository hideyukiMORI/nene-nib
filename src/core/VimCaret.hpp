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

// 行の内容の終わり line_end までの、at から始まる Vim の 1 文字の終わり（次の文字の先頭・
// ADR 0053）。at の後ろの窓だけを読み、文字の終わりが窓の終わりに近ければ窓を倍にして読み直す
// （行の全体は写さない）。at が line_end 以上（CRLF の CR と LF の間を含む）なら at のまま。
// 横送りの押しと INSERT の <Right> が共用する（#1292）。
[[nodiscard]] Offset vim_character_after(const TextBuffer &text, Offset at, Offset line_end);

// 行の内容の始まり line_start から、at の手前の Vim の 1 文字の先頭（ADR 0053）。at は line_start
// より後ろ。at の手前の窓だけを読み、文字の先頭が窓の先頭に当たれば窓を倍にして読み直す。
// vim_character_after と対で、文とタグのテキストオブジェクトが共用する（#1322）。
[[nodiscard]] Offset vim_character_before(const TextBuffer &text, Offset at, Offset line_start);

// 行の内容の終わり line_end までの、at の code point（Vim の gchar_pos）。at が line_end 以上なら
// 0（Vim の NUL）。読むのは at から 1 code point の最長のバイト数だけ（行を写さない）。文と、語・
// 引用符・括弧のテキストオブジェクトが共用する（#1424）。
[[nodiscard]] char32_t vim_code_point_at(const TextBuffer &text, Offset at, Offset line_end);

// at の行の最初の非空白。行が空白だけなら行の最後の文字（空行は行頭）。行単位の削除の
// あとの位置（Vim の規則）。
[[nodiscard]] Offset vim_first_non_blank(const TextBuffer &text, Offset at);

// at から右へ空白（空白と Tab）を越えた最初の文字。at の行の中だけを歩き、空白しか残らなければ
// 行の最後の文字（Vim の ^ と g^ の oneright の歩き・ADR 0147 の決定 8）。at が行の内容の終わり
// ならそのまま。vim_first_non_blank は行頭から歩くこの 1 本。
[[nodiscard]] Offset vim_non_blank_from(const TextBuffer &text, Offset at);

// line の最後の非空白の文字の先頭。空白だけの行と空行は行頭（Vim の g_・ADR 0147 の決定 8）。
[[nodiscard]] Offset vim_last_non_blank(const TextBuffer &text, LineNumber line);

// at の文字が空白（空白と Tab）なら、左へ空白を越えた最後の非空白の文字の先頭。行頭まで空白なら
// 行頭。at の文字が空白でないか、at が行の内容の終わりならそのまま（Vim の g<End> の oneleft の
// 歩き・ADR 0147 の決定 9）。vim_last_non_blank と同じ窓の歩きを共用する。
[[nodiscard]] Offset vim_non_blank_back_from(const TextBuffer &text, Offset at);

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
