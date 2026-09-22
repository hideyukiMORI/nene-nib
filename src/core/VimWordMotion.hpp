#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "VimWordClass.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordStop.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

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

// 文字の種類（Vim の cls()）。語の表はここ 1 つで、テキストオブジェクトも同じ表を引く
// （ADR 0031 の決定 2）。0 は空白、1 は記号、2 は語の文字、それ以外はひらがな等の塊の印。
[[nodiscard]] std::uint32_t vim_character_class(char32_t code, VimWordClass kind) noexcept;

// キャレットの下、無ければ同じ行の後ろにある語の範囲（Issue #100 で実測）。
// `*` / `#` がこの範囲の本文を \<…\> のパターンにする（ADR 0032 の決定 3）。語は同じ種類の
// 文字の連なりで、空白と記号の上では同じ行の次の語を探し、行の中に語が無ければ nullopt。
[[nodiscard]] std::optional<OffsetRange> vim_word_at(const TextBuffer &text, Offset caret);

// テキストオブジェクトが使う語の走査（ADR 0031 の決定 2）。どちらも 1 語ぶんだけ動く。
// vim_word_stop_forward は Vim の fwd_word(1, kind, eol=TRUE)。次の語の頭へ進み、行の終わりで
// 止まる（止まった位置をそのまま返す）。
[[nodiscard]] Offset vim_word_stop_forward(const TextBuffer &text, Offset caret, VimWordClass kind);
// vim_word_object_end は Vim の end_word(1, kind, stop=TRUE, empty=TRUE)。語の末尾へ進み、
// もう末尾にいるなら動かず、空白を飛ぶ途中の空行では空行で止まる。本文が尽きたら nullopt。
[[nodiscard]] std::optional<Offset> vim_word_object_end(const TextBuffer &text, Offset caret,
                                                        VimWordClass kind);

// 後ろ向きの走査（Issue #99 で実測。VISUAL で caret が anchor より小さいときだけ使う）。
// vim_word_object_begin は Vim の bck_word(1, kind, stop=TRUE)。同じ種類の連なりの先頭へ戻り、
// 1 つ手前が違う種類なら動かない。空行に着いたらそこで止まる。本文の先頭で尽きたら nullopt。
[[nodiscard]] std::optional<Offset> vim_word_object_begin(const TextBuffer &text, Offset caret,
                                                          VimWordClass kind);
// vim_word_object_previous_end は Vim の bckend_word(1, kind, eol=TRUE)。手前の語の末尾へ戻り、
// 行をまたいだらその行の内容の終わり（NUL の桁）で止まる。本文の先頭で尽きたら nullopt。
[[nodiscard]] std::optional<Offset> vim_word_object_previous_end(const TextBuffer &text,
                                                                 Offset caret, VimWordClass kind);
} // namespace nenenib::core
