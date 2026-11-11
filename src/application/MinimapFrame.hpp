#pragma once

#include "LineNumber.hpp"
#include "MinimapBand.hpp"
#include "MinimapRows.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace nenenib::application
{
// ミニマップの表示値（ADR 0171 の決定 4）。本文の全部は写さず、窓の行の塊を借りる（rows）。top は
// 面の最上段の行、first_visible と visible は見えている範囲の面の行。base は前の frame を作ったとき
// の rows の serial で、band はそれから後に替わった帯（無ければ値なし）。renderer は自分の bitmap
// が rows の同じ値の base の絵なら band だけを描き直し、そうでなければ窓ごと描く。
struct MinimapFrame
{
    std::shared_ptr<const MinimapRows> rows;
    core::LineNumber top;
    core::LineNumber first_visible;
    std::size_t visible;
    std::uint64_t base;
    std::optional<core::MinimapBand> band;
};
} // namespace nenenib::application
