#pragma once

#include "SplitLayout.hpp"
#include "SplitShare.hpp"

namespace nenenib::core
{
// 本文の枠の配置と境界の位置を 1 つに畳んだ値（ADR 0148 の 2026-11-22 の追補・#1941）。
// pane_layouts の引数（tidy の引数 4 つの上限に収める）。layout が none なら share は読まない。
// どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct PaneSplit
{
    SplitLayout layout = SplitLayout::none;
    SplitShare share{};
};

[[nodiscard]] constexpr bool operator==(const PaneSplit &left, const PaneSplit &right) noexcept
{
    return left.layout == right.layout && left.share == right.share;
}
} // namespace nenenib::core
