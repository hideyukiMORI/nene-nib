#pragma once

#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordStop.hpp"

#include <cstddef>

namespace nenenib::core
{
// Vim の語の移動（ADR 0012 の決定 5）。語の切れ目は文字の種類（空白・記号・語の文字・
// ひらがな・カタカナ・漢字…）が変わるところで、空行そのものも 1 つの語として止まる。
// 既存の moved_caret の語は空白だけで切るので別物。あちらは通常モードのまま変えない。
[[nodiscard]] Offset vim_next_word(const TextBuffer &text, Offset caret, std::size_t count,
                                   VimWordStop stop);
[[nodiscard]] Offset vim_previous_word(const TextBuffer &text, Offset caret, std::size_t count);

// e（Vim の end_word）。語の末尾の文字へ進み、空行は素通りして行をまたぐ。
// 走査が本文の終わりで尽きたときは、Vim と同じくそこで止まった位置をそのまま返す
// （オペレータの後ろでは、その位置までが範囲になる＝ $de が最後の 1 文字だけを消す理由）。
[[nodiscard]] Offset vim_word_end(const TextBuffer &text, Offset caret, std::size_t count,
                                  VimWordEndStop stop);
} // namespace nenenib::core
