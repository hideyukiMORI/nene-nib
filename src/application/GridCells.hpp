#pragma once

#include "GridPoint.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextBuffer.hpp"
#include "VirtualColumnChunk.hpp"
#include "VirtualColumnCursor.hpp"
#include "VirtualColumnIndex.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 格子の升（ADR 0140 の決定 4・5）を、本文が持つ仮想桁の索引（TextBuffer::virtual_columns）から
// 答える。索引は借りるだけで作らない。升は桁に「2 升以上の文字（Tab を除く）が画面行の右端に
// 収まらず次の画面行へ送った空き」を足したもので、送りは幅 columns に依るので索引には持たせない。
// 行の中を索引の塊の境目で切り、plain の塊（表示できる ASCII と '\n' だけ）の部分は長さを足すだけ、
// そうでない部分だけ 1 文字ずつ置いて送りを数える（置き方は core の grid_placed_cell の 1 本）。
// 1 回の位置解決の間だけ作る短い値で、1 文字ずつ歩いた部分の数を scanned に数える。同じ行への
// 問いは、前の問いで進めた塊の始まりの累積を覚えて使い回す（本文と索引は値の寿命の間変わらない）。
class GridCells final
{
  public:
    // index は text が持つ索引。columns は 1 画面行の升の数（0 は 1 に丸める）。
    GridCells(const core::VirtualColumnIndex &index, const core::TextBuffer &text,
              std::size_t columns) noexcept;

    // 行 line の行頭から at（行の中へ丸める）の手前までの累積。cell は送りの空きを含む。
    [[nodiscard]] core::VirtualColumnCursor cursor_at(core::LineNumber line, core::Offset at);
    // at（code point の先頭）の文字が始まる画面行と升。幅 0 の文字は直前の文字の升。行の終わりが
    // 画面行の右端にちょうど達したときは、その画面行の右端（cell == columns）。
    [[nodiscard]] core::GridPoint point_of(core::LineNumber line, core::Offset at);
    // 行 line の画面行 point.row の升 point.cell を占める文字の先頭。送りの空きは送られた文字、
    // 行の終わりより後ろは行の内容の終わり。
    [[nodiscard]] core::Offset byte_of(core::LineNumber line, core::GridPoint point);
    // 行 line の画面行の数。空行も 1 行。
    [[nodiscard]] std::size_t rows(core::LineNumber line);

    [[nodiscard]] std::size_t columns() const noexcept;
    // この値が 1 文字ずつ歩いた部分（plain でない塊の、行に掛かる部分）の数。
    [[nodiscard]] std::size_t scanned() const noexcept;

  private:
    [[nodiscard]] std::size_t first_chunk(std::size_t at) const noexcept;
    // 行の [begin, stop) に掛かる塊 chunk の部分。掛からなければ空。
    [[nodiscard]] core::OffsetRange part_of(std::size_t chunk, std::size_t begin,
                                            std::size_t stop) const noexcept;
    void remember(core::LineNumber line);
    void noted(std::size_t ordinal, std::size_t chunk, core::OffsetRange range,
               const core::VirtualColumnCursor &state);
    // 塊 chunk の部分 range を state から進める。target の升を占める文字があればその先頭を返す。
    // plain の部分は長さを足すだけ、一様な塊（VirtualColumnChunk の uniform_*）は閉じた式、送りの
    // 対象の無い塊の全体は要約の終わりの桁から足す（target が掛からないとき）。そのほかは 1 文字
    // ずつ置いて scanned を 1 つ数える（#1711）。
    [[nodiscard]] std::optional<std::size_t> reached(core::OffsetRange range, std::size_t chunk,
                                                     core::VirtualColumnCursor &state,
                                                     std::size_t target);
    [[nodiscard]] std::optional<std::size_t> uniform_reached(core::OffsetRange range,
                                                             const core::VirtualColumnChunk &part,
                                                             core::VirtualColumnCursor &state,
                                                             std::size_t target) const noexcept;
    [[nodiscard]] std::optional<std::size_t>
    walked(core::OffsetRange range, core::VirtualColumnCursor &state, std::size_t target);
    [[nodiscard]] core::Offset previous_in_line(core::LineNumber line, core::Offset at) const;

    const core::VirtualColumnIndex &index_;
    const core::TextBuffer &text_;
    std::size_t columns_;
    std::size_t scanned_ = 0;
    // 最後に問われた行（remembered_line_）の、行頭と、行の中で終わりまで進めた塊の次の塊の
    // 始まりの累積（行頭の塊から順に）。同じ値への次の問いは、ここから歩く（frame の中で行を 1 回
    // だけ歩く・#1711）。
    std::size_t remembered_line_ = 0;
    std::vector<core::VirtualColumnCursor> starts_;
};
} // namespace nenenib::application
