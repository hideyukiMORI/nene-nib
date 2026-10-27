#pragma once

#include "GridPoint.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"

#include <cstddef>

namespace nenenib::core
{
class TextBuffer;
class VirtualColumnIndex;

// 格子の決め方（ADR 0140 の決定 4）。索引は VirtualColumnIndex::for_grid で作った値。

// 升の和が cells の行の画面行の数。空行も 1 行。cells は「幅 columns のときの累積」（送りの空きを
// 含む・VirtualColumnIndex::cursor_at の cell）。
[[nodiscard]] std::size_t grid_rows(std::size_t cells, std::size_t columns) noexcept;

// 升 cell（行頭からの累積）に cells 升の文字（Tab 以外）を置くときに始まる升。2 升以上の文字が
// 画面行の残りに収まらなければ次の画面行の先頭へ送る（columns より大きい文字は送らない）。
[[nodiscard]] std::size_t grid_placed_cell(std::size_t cell, std::size_t cells,
                                           std::size_t columns) noexcept;

// at（code point の先頭）の文字が始まる画面行と升。幅 0 の文字は直前の文字の升。行の終わりが
// 画面行の右端にちょうど達したときは、その画面行の右端（cell == columns）。行の内容の終わりは
// TextBuffer::line_end。製品の経路は application の GridCells で、これは試験の参照（#942）。
[[nodiscard]] GridPoint grid_cell_of(const VirtualColumnIndex &index, const TextBuffer &text,
                                     Offset at);

// 行 line の画面行 point.row の升 point.cell を占める文字の先頭。送りの空きは送られた文字、
// 行の終わりより後ろは行の内容の終わり。
[[nodiscard]] Offset grid_byte_of(const VirtualColumnIndex &index, const TextBuffer &text,
                                  LineNumber line, GridPoint point);
} // namespace nenenib::core
