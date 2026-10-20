#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimTextObjectOutcome.hpp"
#include "VimTextObjectRequest.hpp"

#include <cstddef>

namespace nenenib::core
{
// 段落のテキストオブジェクト `ip` `ap`（ADR 0031 の決定 9・Issue #729）。範囲関数
// vim_text_object_range の段落の枝で、オペレータの後ろでも VISUAL でもこの答えを使う（ARC-001）。
//
// 段落は空白行（空か、space と tab だけの行）で区切る。`ip` は段落か空白行の並び 1 つ、`ap` は
// 段落と後ろの空白行（無ければ前の空白行）。回数は並びの数。範囲は常に行単位で、VISUAL は行単位
// VISUAL へ移る。選択が 2 行以上（か行単位 VISUAL の 1 行の段落）なら、今の選択を caret の向きへ
// 並びの数だけ広げる。どれも Vim 9.1 で実測。行は写さず、行頭からの窓だけを読む。純関数。
[[nodiscard]] VimTextObjectOutcome vim_paragraph_object_range(const TextBuffer &text,
                                                              const Selection &selection,
                                                              VimTextObjectRequest request,
                                                              std::size_t count);
} // namespace nenenib::core
