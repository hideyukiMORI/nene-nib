#pragma once

#include "Offset.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRequest.hpp"

#include <cstddef>
#include <optional>

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

// 文の移動 `(` `)` の着地（Vim の findsent(dir, count)・Issue #1316・ADR 0031 の決定 10 の追補）。
// 回数 1 つぶんの歩きは `is` `as` と同じ境の関数（findsent(dir, 1)）で、回数だけ繰り返す。文の
// 終わりを探す歩きが本文の端で尽きたとき、残りの回数があれば nullopt（Vim の FAIL・動かない）、
// 最後の 1 つなら尽きた所（前向きは最後の行の内容の終わり＝Vim が NUL を置く桁）。NORMAL の寄せ
// （adjust_cursor）は呼び出し元が行う。純関数。
[[nodiscard]] std::optional<Offset> vim_sentence_motion(const TextBuffer &text, Offset caret,
                                                        bool forward, std::size_t count);
} // namespace nenenib::core
