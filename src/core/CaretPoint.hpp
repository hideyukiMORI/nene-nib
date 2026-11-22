#pragma once

namespace nenenib::core
{
// キャレットの動きの 1 点（物理画素・小数。ADR 0191）。どちらの座標も単独で妥当なので公開
// aggregate（ADR 0007）。
struct CaretPoint
{
    float x;
    float y;
};

[[nodiscard]] constexpr bool operator==(const CaretPoint &left, const CaretPoint &right) noexcept
{
    return left.x == right.x && left.y == right.y;
}
} // namespace nenenib::core
