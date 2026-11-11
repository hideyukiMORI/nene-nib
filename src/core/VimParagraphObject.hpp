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
// 段落のテキストオブジェクト `ip` `ap`（ADR 0031 の決定 9・Issue #729）。範囲関数
// vim_text_object_range の段落の枝で、オペレータの後ろでも VISUAL でもこの答えを使う（ARC-001）。
//
// 段落は空白行（空か、space と tab だけの行）で区切り、段落の行の並びは段落の始まりの行
// （vim_paragraph_start・nroff のマクロの行と改ページ・#1359）でも切れる。`ip` は段落か空白行の並び
// 1 つ、`ap` は
// 段落と後ろの空白行（無ければ前の空白行）。回数は並びの数。範囲は常に行単位で、VISUAL は行単位
// VISUAL へ移る。選択が 2 行以上（か行単位 VISUAL の 1 行の段落）なら、今の選択を caret の向きへ
// 並びの数だけ広げる。どれも Vim 9.1 で実測。行は写さず、行頭からの窓だけを読む。純関数。
[[nodiscard]] VimTextObjectOutcome vim_paragraph_object_range(const TextBuffer &text,
                                                              const Selection &selection,
                                                              VimTextObjectRequest request,
                                                              std::size_t count);

// 段落の移動 `{` `}` の着地（Vim の findpar(dir, count, NUL, FALSE)・Issue #1316・ADR 0031 の決定 9
// の追補）。境は空の行（と段落の始まりの行）で、`ip` `ap` の空白行（space と tab
// だけの行も含む・Vim の current_par の linewhite）とは Vim
// でも別の判定。空でない行を越えた後の空の行に行頭で着き、本文の端で尽きたら 残りの回数があれば
// nullopt（Vim の FAIL・動かない）、最後の 1 つなら端の行（前向きの最後の行は
// 最後の文字で、オペレータの範囲はその文字まで inclusive）。nroff のマクロの行と改ページ（form
// feed）で始まる行も境で、空でないのでその行自身から越えた数に入る（vim_paragraph_start・#1359）。
// 行は写さない。純関数。
[[nodiscard]] std::optional<Offset> vim_paragraph_motion(const TextBuffer &text, Offset caret,
                                                         bool forward, std::size_t count);
} // namespace nenenib::core
