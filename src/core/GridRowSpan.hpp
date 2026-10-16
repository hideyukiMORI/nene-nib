#pragma once

#include <cstddef>

namespace nenenib::core
{
// 格子の 1 画面行に載る本文の範囲（ADR 0140 の決定 5・試作）。最初の升がこの画面行に入る文字だけを
// 持つ。バイトと code point は行の中の位置（半開区間）。lead は前の画面行から続く Tab の空きの升数
// （最初の文字の左に空ける）、first_virtual は最初の文字の仮想桁（Tab の止まりを決める）。
struct GridRowSpan
{
    std::size_t byte_begin;
    std::size_t byte_end;
    std::size_t column_begin;
    std::size_t column_end;
    std::size_t lead;
    std::size_t first_virtual;
};
} // namespace nenenib::core
