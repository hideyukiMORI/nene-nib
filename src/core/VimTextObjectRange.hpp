#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRequest.hpp"

#include <cstddef>

namespace nenenib::core
{
// テキストオブジェクトが覆う本文の範囲（ADR 0031 の決定 2）。オペレータの後ろでも VISUAL でも
// この 1 本が決めた答えを使う（ARC-001）。純関数。
//
// selection は NORMAL では畳んだ位置（anchor == caret）、VISUAL では今の選択。選択があるときは
// Vim と同じく「今の選択を 1 つぶん広げる」意味になり、caret が anchor より小さい後ろ向きの
// 選択では、語は anchor を動かさずに caret だけを 1 単位ずつ手前へ運ぶ（Issue #99 で実測）。
// 範囲は文字単位が既定で、`i(` の開きの直後が改行かつ閉じの前が空白だけのときだけ行単位になる
// （決定 7・Vim 9.1 で実測）。exclusive 補正は通さない。
//
// 範囲にならなければ VimTextObjectCancel で、そこに残る選択（＝取消のあとのキャレット）まで
// 返す。回数が本文で尽きた語は Vim と同じく走査が止まった端へキャレットを運ぶ（ADR 0031 の補足）。
[[nodiscard]] VimTextObjectOutcome vim_text_object_range(const TextBuffer &text,
                                                         const Selection &selection,
                                                         VimTextObjectRequest request,
                                                         std::size_t count);
} // namespace nenenib::core
