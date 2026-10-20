#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRequest.hpp"

#include <cstddef>

namespace nenenib::core
{
// 文のテキストオブジェクト `is` `as`（ADR 0031 の決定 10・Issue #729）。範囲関数
// vim_text_object_range の文の枝で、オペレータの後ろでも VISUAL でもこの答えを使う（ARC-001）。
//
// 文は `.` `!` `?` の後に閉じ括弧類 `)` `]` `"` `'` が続いてよく、その後に空白か行末が来る所で
// 終わる。空行（段落の境）でも終わる。`is` は文か文の間の空白の並び 1 つ、`as` は文と後ろの空白
// （無ければ前の空白）。回数は並びの数（`as` は 2 つずつ）。範囲は文字単位で、文の後ろの改行まで
// 届くときは Vim の exclusive の規則で行単位になり得る。VISUAL は文字単位 VISUAL へ移り、
// 選択が 1 文字より大きければ caret の向きへ広げる。どれも Vim 9.1 で実測。
// 走査はキャレットの周りの窓だけを読み、行を写さない。純関数。
[[nodiscard]] VimTextObjectOutcome vim_sentence_object_range(const TextBuffer &text,
                                                             const Selection &selection,
                                                             VimTextObjectRequest request,
                                                             std::size_t count);
} // namespace nenenib::core
