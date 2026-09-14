#pragma once

#include <cstdint>

namespace nenenib::core
{
// 3 つのチャンネルは互いに独立して妥当なので、検証の要らない公開 aggregate にする（ADR 0007）。
struct RgbColor
{
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

// 等値は非メンバで書く。メソッドを 1 つでも持つと公開メンバが clang-tidy
// misc-non-private-member-variables-in-classes に落ちるため（Phase 0 T1-tidy-public-with-method）。
[[nodiscard]] constexpr bool operator==(const RgbColor &left, const RgbColor &right) noexcept
{
    return left.red == right.red && left.green == right.green && left.blue == right.blue;
}
} // namespace nenenib::core
