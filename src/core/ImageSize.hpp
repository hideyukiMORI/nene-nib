#pragma once

#include <cstdint>

namespace nenenib::core
{
// 画像の大きさ（画素か DIP・単位は使う所が決める）。幅と高さは単独で妥当な値なので公開 aggregate
// （CPP-003）。比較は非メンバー（ADR 0155 の追補 49・#1381）。
struct ImageSize
{
    std::uint32_t width;
    std::uint32_t height;
};

[[nodiscard]] constexpr bool operator==(ImageSize left, ImageSize right) noexcept
{
    return left.width == right.width && left.height == right.height;
}
} // namespace nenenib::core
