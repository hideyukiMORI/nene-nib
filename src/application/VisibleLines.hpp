#pragma once

#include "SplitExtent.hpp"
#include "TextViewGeometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::application
{
// 窓の大きさが変わったときに UI が知らせる「本文が何行入るか」。行数の所有は application。
// window_width / window_height は窓の client の物理画素で、Ctrl+P の面と `:` の補完の面の
// 見えている行数を application が決めるのに読む（ADR 0150 の決定 1）。0 は寸法が届く前で、行数は
// 上限のまま。explorer_lines はエクスプローラーの面の高さ（窓の client の本文の帯）に入る行数で、
// 先頭のルート名の行を含む（#1125・ADR 0162 の追補）。面が出ているかに依らず数え、分割
// （ADR 0148）でも変わらない。0 は寸法が届く前で、application は本文の見えている行数を使う。
struct VisibleLines
{
    std::size_t lines;
    core::TextViewGeometry geometry{};
    std::int32_t window_width = 0;
    std::int32_t window_height = 0;
    std::size_t explorer_lines = 0;
    // 2 枠のときの境界の寸法（ADR 0148 の 2026-11-22 の追補の決定 4・鍵で大きさを変えるときに読む）
    // と、もう 1 つの枠の寸法と行数（決定 5・両枠の大きさは違ってよい）。1 枠では値なしと 0。
    // other_lines が 0 なら application はもう 1 つの枠にも操作中の枠の値を使う（寸法が届く前・替え
    // 玉の試験）。
    std::optional<core::SplitExtent> split = std::nullopt;
    core::TextViewGeometry other_geometry{};
    std::size_t other_lines = 0;
};
} // namespace nenenib::application
