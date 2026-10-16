#pragma once

#include "GridCellSpan.hpp"
#include "GridLineSource.hpp"
#include "GridRowView.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::application
{
// 1 行の見えている画面行を格子の升で切り出す（ADR 0140 の決定 5）。始まりは GridCells の
// byte_of（画面行 → 本文の位置）と升の累積 cursor_at（本文の索引を借り、ASCII の塊は割り算だけ）
// で、そこから見えている画面行の終わりまでだけ本文を読む。各文字の升は core の column_advance と
// grid_placed_cell（升の累積と同じ 規則）で決め、ui
// は数えない。選択・検索の当たり・キャレット・変換中の文字列は、本文の位置の
// 範囲を画面行ごとの升の範囲に写す。
class GridLineRows final
{
  public:
    explicit GridLineRows(const GridLineSource &source);
    // 作った画面行を rows の後ろへ移す。
    void append_to(std::vector<GridRowView> &rows);

  private:
    // 本文の 1 文字がある画面行で占める片: 本文の位置・画面行の中の始まりの升・終わりの升。
    using Piece = std::array<std::size_t, 3>;

    void walk();
    void step(core::Offset at, char32_t value, std::string_view drawn);
    void put(core::Offset at, std::size_t start, std::size_t cells, std::string_view drawn);
    void blank(std::size_t from, std::size_t to, std::optional<core::Offset> owner);
    void attach(core::Offset at, std::string_view drawn);
    void emit(std::size_t row, std::size_t cells, std::string_view drawn,
              std::optional<core::Offset> owner);
    void finish();
    void mark();
    void mark_row(std::size_t index, std::size_t last);
    void mark_caret();
    void compose();
    [[nodiscard]] std::optional<GridCellSpan> span_of(std::size_t index,
                                                      const core::OffsetRange &range) const;
    [[nodiscard]] bool shown(std::size_t row) const noexcept;

    const GridLineSource &source_;
    std::size_t columns_;
    // 歩いた所までの累積（仮想桁と升）と、幅 0 の文字を載せる画面行（直前の文字の画面行）。
    std::size_t column_ = 0;
    std::size_t cell_ = 0;
    std::size_t last_row_ = 0;
    bool stopped_ = false;
    // キャレットがこの行の見えている画面行にあるか。
    bool caret_shown_ = false;
    std::vector<GridRowView> rows_;
    std::vector<std::vector<Piece>> pieces_;
    std::vector<std::size_t> ends_;
};
} // namespace nenenib::application
