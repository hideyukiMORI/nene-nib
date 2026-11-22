#pragma once

#include <cstdint>

namespace nenenib::core
{
// ステータスバーの右端の言語サーバーの欄に何を出すか（ADR 0161 の決定 4・ADR 0160 の 2026-11-22 の
// 追補）。none は欄を出さない（幅 0）、counts は診断の件数（96 DIP）、progress は言語サーバーの
// 進み具合か起動中の 1 行（件数より広い幅）。枠は 1 つのままで、件数と進み具合は同時に出さない。
enum class StatusServerItem : std::uint8_t
{
    none,
    counts,
    progress
};
} // namespace nenenib::core
