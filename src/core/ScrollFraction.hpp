#pragma once

#include <cstddef>

namespace nenenib::core
{
// 本文の縦の送りの端数の単位（#1898・ADR 0009 の決定 6 の 2026-11-20 の追補）。1 画面行を 120 に
// 分ける（WM_MOUSEWHEEL の WHEEL_DELTA と同じ数で、1 刻みの行数を掛けた delta がそのまま端数に
// なる）。ScrollState::fraction は 0 以上この値未満。
inline constexpr std::size_t scroll_fraction_units = 120;
} // namespace nenenib::core
