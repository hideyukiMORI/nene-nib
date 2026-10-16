#pragma once

#include <cstddef>

namespace nenenib::application
{
// 格子の 1 画面行の表示値（ADR 0140 の決定 5・試作）。LineView の text / display はこの画面行の
// 切れ端で、column_begin は切れ端の先頭の本文の桁（0 始まり）。lead は左に空ける升、cell は 1 升の
// 物理画素。最初の画面行だけが行番号を描き、最後の画面行だけが改行の印を持つ。
struct GridRowView
{
    std::size_t column_begin;
    std::size_t lead;
    float cell;
    bool first_row;
    bool last_row;
};
} // namespace nenenib::application
