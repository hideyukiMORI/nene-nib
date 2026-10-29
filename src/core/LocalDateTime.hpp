#pragma once

#include <cstdint>

namespace nenenib::core
{
// 地方時の日時（ADR 0175 の決定 1）。値は ClockPort の実装（src/adapters/win32）が OS から写し、
// core と application は時刻を読まずにこの値だけを受ける（ARC-007）。欄は互いに独立して写すだけ
// なので、検証の要らない公開 aggregate にする（RgbColor と同じ形）。
struct LocalDateTime
{
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;
    std::uint8_t hour;
    std::uint8_t minute;
    std::uint8_t second;
};

// 等値は非メンバで書く（CPP-003・公開 aggregate がメソッドを持つと lint が落ちる）。
[[nodiscard]] constexpr bool operator==(const LocalDateTime &left,
                                        const LocalDateTime &right) noexcept
{
    return left.year == right.year && left.month == right.month && left.day == right.day &&
           left.hour == right.hour && left.minute == right.minute && left.second == right.second;
}
} // namespace nenenib::core
