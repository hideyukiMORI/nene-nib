#pragma once

#include "Offset.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimMotionRange.hpp"
#include "VimTextObjectRequest.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// テキストオブジェクトが覆う本文の範囲（ADR 0031 の決定 2）。オペレータの後ろでも VISUAL でも
// この 1 本が決めた範囲を使う（ARC-001）。純関数。
//
// selection は NORMAL では畳んだ位置（anchor == caret）、VISUAL では今の選択。選択があるときは
// Vim と同じく「今の選択を 1 つぶん広げる」意味になる（`viwiw` / `vi(i(`・ADR 0031 の決定 4）。
// 範囲は文字単位が既定で、`i(` の開きの直後が改行かつ閉じの前が空白だけのときだけ行単位になる
// （決定 7・Vim 9.1 で実測）。exclusive 補正は通さない。見つからなければ nullopt で取消。
[[nodiscard]] std::optional<VimMotionRange> vim_text_object_range(const TextBuffer &text,
                                                                  const Selection &selection,
                                                                  VimTextObjectRequest request,
                                                                  std::size_t count);

// 範囲の最後の文字の位置（VISUAL の caret になる）。行単位の範囲では最後の行の内容の終わり
// （Vim が NUL を置く桁）で、そこに caret を置くと選択が改行まで届く（実測）。
[[nodiscard]] Offset vim_text_object_caret(const TextBuffer &text, const VimMotionRange &range);
} // namespace nenenib::core
