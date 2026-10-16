#pragma once

#include "DisplayCaret.hpp"
#include "CaretAffinity.hpp"
#include "GridCheckpoint.hpp"
#include "GridPoint.hpp"
#include "GridRowSpan.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 格子表示の 1 行（ADR 0140 の決定 4・試作）。行の本文の写しと、64 KiB の塊ごとの累積を持つ純粋な
// 値。各文字の升数は仮想桁（ADR 0034）と同じ表で、幅 1 は 1 升・幅 2 は 2 升・Tab は次の 8 の倍数まで・
// 制御文字は表示の長さ（`^M` 2・`<85>` 4・`<200b>` 6）・幅 0 は直前の文字の升に重ねる。2 升以上の
// 文字（Tab を除く）が画面行の残りに収まらないときは、残りを空けて次の画面行へ送る（Vim と同じ）。
// ASCII だけの塊は位置を割り算で出し、ほかの塊は塊の先頭から歩く。
class GridLine final
{
  public:
    [[nodiscard]] static GridLine build(std::string text, std::size_t columns);

    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] std::size_t columns() const noexcept;
    [[nodiscard]] std::size_t cells() const noexcept;
    [[nodiscard]] std::size_t code_points() const noexcept;
    [[nodiscard]] std::size_t rows() const noexcept;
    [[nodiscard]] std::size_t checkpoint_count() const noexcept;
    // 本文の位置 column（0 始まりの code point・末尾は code_points()）が画面に出る升。trailing は
    // 直前の文字の終わり（画面行の右端に立つキャレット）。
    [[nodiscard]] GridPoint point_of(std::size_t column, CaretAffinity affinity) const;
    // 画面行 row の左端から cell 升（小数）の点に最も近い文字の境目。行の右端を越えたら trailing。
    [[nodiscard]] DisplayCaret hit(std::size_t row, float cell) const;
    [[nodiscard]] GridRowSpan row_span(std::size_t row) const;

  private:
    GridLine(std::string text, std::size_t columns);
    [[nodiscard]] GridCheckpoint at_column(std::size_t column) const;
    [[nodiscard]] GridCheckpoint at_cell(std::size_t cell) const;
    [[nodiscard]] std::size_t start_of(const GridCheckpoint &state) const noexcept;

    std::string text_;
    std::size_t columns_;
    std::vector<GridCheckpoint> checkpoints_;
    GridCheckpoint end_;
};

// 画面行の数（決定 4）。升が 0 でも 1 行。
[[nodiscard]] std::size_t grid_rows(std::size_t cells, std::size_t columns) noexcept;
// 本文の幅（物理画素）に入る升の数。最低 1。
[[nodiscard]] std::size_t grid_columns(std::int32_t width, float cell) noexcept;
} // namespace nenenib::core
