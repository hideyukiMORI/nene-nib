#pragma once

#include "OffsetRange.hpp"

#include <cstddef>

namespace nenenib::core
{
// 矩形が 1 行を覆う形（ADR 0035 の決定 2）。矩形の端が Tab や全角の途中に掛かると、固定 Vim は
// その文字を丸ごと外して掛かった桁ぶんの空白に置き換える（Issue #112 で実測）。だから 1 行は
// 「置き換える本文」と「丸ごと入っている本文」と、その前後に足す空白の数で表す。
//
// `range` は d・r が置き換える本文（端で切れた文字も含む）。空なら本文は動かない。
// `inside` は丸ごと矩形に入っている本文で、レジスタに入る本文はこれだけ。
// `lead` / `tail` は `inside` の前後にレジスタへ入る空白（切れた文字の内側の桁と、行が矩形より
// 手前で終わるときの幅ぶんの埋め）。`keep_lead` / `keep_tail` は削除や置換のあと本文に残す空白
// （切れた文字のうち矩形の外側の桁）。
struct VimBlockLine
{
    OffsetRange range;
    OffsetRange inside;
    std::size_t lead;
    std::size_t tail;
    std::size_t keep_lead;
    std::size_t keep_tail;
};
} // namespace nenenib::core
