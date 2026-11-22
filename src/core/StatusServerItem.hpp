#pragma once

#include <cstdint>

namespace nenenib::core
{
// ステータスバーの右端の言語サーバーの欄に何を出すか（ADR 0161 の決定 4・ADR 0160 の 2026-11-22 の
// 追補）。none は欄を出さない（幅 0）、counts は言語サーバーの受けていない文書の診断の件数
// （96 DIP）、server は言語サーバーの受けている文書の欄（件数と進み具合か起動中の 1 行を並べる・
// 件数より広い一定の幅で、進み具合の有無で右の項目を動かさない・#1950 のレビュー R2 / R3）。枠は
// 1 つのまま。
enum class StatusServerItem : std::uint8_t
{
    none,
    counts,
    server
};
} // namespace nenenib::core
