#pragma once

#include "PaneDimension.hpp"
#include "PaneResize.hpp"

#include <cstdint>

namespace nenenib::core
{
// 鍵で枠の大きさを変える 1 回（ADR 0148 の 2026-11-22 の追補・#1941）。Vim の `Ctrl-w > < + - = _
// |` と Ex の `:resize` `:vertical resize`、通常モードの操作（F1 の一覧）が同じ値を作り、
// controller の 1 本（resize_pane）が sized_share で比に直す。cells は升（左右なら文字の幅・上下な
// ら行の高さ）の数で、grow / shrink / set だけが読む。dimension が分割の向きと合わなければ何もしな
// い（even を除く）。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct PaneSizing
{
    PaneResize resize;
    PaneDimension dimension;
    std::int32_t cells = 1;
};

[[nodiscard]] constexpr bool operator==(const PaneSizing &left, const PaneSizing &right) noexcept
{
    return left.resize == right.resize && left.dimension == right.dimension &&
           left.cells == right.cells;
}
} // namespace nenenib::core
