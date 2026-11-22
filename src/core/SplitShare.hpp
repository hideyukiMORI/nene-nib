#pragma once

#include <cstdint>

namespace nenenib::core
{
// 境界の位置（ADR 0148 の 2026-11-22 の追補・#1941）。位置の順の first の枠（左か上）が、境界を除い
// た帯の長さ（左右なら幅・上下なら高さ）のうち first / whole を取る比。窓の大きさが変わっても比のま
// ま保つ。既定の {1, 2} は等分で、pane_layouts は等分を今までの式（両枠に同じ長さ・余りは境界）で置
// く。whole が 0 以下か first が範囲の外の値は等分として読む。どの欄も単独で妥当な値なので公開
// aggregate（CPP-003）。作るのは split_share_of の 1 本（境界の位置から）と既定値。
struct SplitShare
{
    std::int32_t first = 1;
    std::int32_t whole = 2;
};

[[nodiscard]] constexpr bool operator==(SplitShare left, SplitShare right) noexcept
{
    return left.first == right.first && left.whole == right.whole;
}

// 等分か（比が 1 / 2・範囲の外の値も等分として読む）。
[[nodiscard]] constexpr bool is_even_share(SplitShare share) noexcept
{
    return share.whole <= 0 || share.first < 0 || share.first > share.whole ||
           static_cast<std::int64_t>(share.first) * 2 == share.whole;
}
} // namespace nenenib::core
