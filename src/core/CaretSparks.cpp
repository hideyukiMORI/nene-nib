// 打ったときの粒の純関数（ADR 0192 の追補・#1979）。時計・乱数・OS に触れない（ARC-007）。
#include "CaretSparks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
namespace
{
// 扇の 9 本の向き（真上から−1.1・−0.825・…・+1.1 rad の単位ベクトル。y は下が正なので上向きは負）。
// 見本の「−π/2 ± 1.1 rad」を、三角関数を使わずに表の線形補間で引く。
constexpr std::array<CaretPoint, 9> fan{{
    {-0.8912F, -0.4536F},
    {-0.7346F, -0.6785F},
    {-0.5227F, -0.8525F},
    {-0.2715F, -0.9624F},
    {0.0F, -1.0F},
    {0.2715F, -0.9624F},
    {0.5227F, -0.8525F},
    {0.7346F, -0.6785F},
    {0.8912F, -0.4536F},
}};

constexpr float slowest = 40.0F;
constexpr float speed_span = 90.0F;
constexpr float gravity = 260.0F;
constexpr float smallest = 2.0F;
constexpr float size_span = 2.5F;
constexpr float per_second = 1000.0F;
// ハッシュの 10 ビットずつを 0〜1 の割合に読む。
constexpr std::uint32_t field_mask = 0x3FFU;
constexpr float field_top = 1023.0F;
constexpr unsigned speed_shift = 10U;
constexpr unsigned size_shift = 20U;
// 上の 4 ビットが 6 未満（16 のうち 6・約 4 割）なら 2 つ目の色。
constexpr unsigned tint_shift = 28U;
constexpr std::uint32_t tinted_below = 6U;

// 32 ビットの整数の混ぜ（lowbias32・決定的）。
[[nodiscard]] std::uint32_t mixed(std::uint32_t value) noexcept
{
    constexpr std::uint32_t first = 0x7FEB352DU;
    constexpr std::uint32_t second = 0x846CA68BU;
    constexpr unsigned half = 16U;
    constexpr unsigned middle = 15U;
    value ^= value >> half;
    value *= first;
    value ^= value >> middle;
    value *= second;
    value ^= value >> half;
    return value;
}

[[nodiscard]] std::uint32_t spark_hash(std::uint32_t serial, std::size_t index) noexcept
{
    constexpr std::uint32_t golden = 0x9E3779B9U;
    constexpr std::uint32_t odd = 0x85EBCA6BU;
    return mixed((serial * golden) ^ (static_cast<std::uint32_t>(index + 1U) * odd));
}

[[nodiscard]] float share_of(std::uint32_t hash, unsigned shift) noexcept
{
    return static_cast<float>((hash >> shift) & field_mask) / field_top;
}

// 扇の割合（0〜1）の向き。隣り合う 2 本の線形補間。
[[nodiscard]] CaretPoint direction_of(float share) noexcept
{
    const float place = share * static_cast<float>(fan.size() - 1U);
    const auto lower = std::min(static_cast<std::size_t>(place), fan.size() - 2U);
    const float rest = place - static_cast<float>(lower);
    const auto &from = fan.at(lower);
    const auto &to = fan.at(lower + 1U);
    return CaretPoint{from.x + ((to.x - from.x) * rest), from.y + ((to.y - from.y) * rest)};
}

// 寿命の過ぎた束を前から捨てる（束は生まれた順なので、前ほど古い）。
void expire(CaretSparkTrack &track) noexcept
{
    std::size_t dead = 0;
    while (dead < track.count && track.now - track.bursts.at(dead).born >= caret_spark_life)
    {
        ++dead;
    }
    for (std::size_t index = dead; index < track.count; ++index)
    {
        track.bursts.at(index - dead) = track.bursts.at(index);
    }
    track.count -= dead;
    track.alive = track.count != 0;
}

// 束を後ろに足す（満ちていれば最も古い束を捨てる）。
void push(CaretSparkTrack &track, const CaretSparkBurst &burst) noexcept
{
    if (track.count == track.bursts.size())
    {
        for (std::size_t index = 1; index < track.count; ++index)
        {
            track.bursts.at(index - 1U) = track.bursts.at(index);
        }
        --track.count;
    }
    track.bursts.at(track.count) = burst;
    ++track.count;
    track.alive = true;
}
} // namespace

std::optional<CaretSpark> caret_spark_at(std::uint32_t serial, std::size_t index,
                                         std::uint32_t age) noexcept
{
    if (age >= caret_spark_life)
    {
        return std::nullopt;
    }
    const std::uint32_t hash = spark_hash(serial, index);
    const CaretPoint toward = direction_of(share_of(hash, 0U));
    const float speed = slowest + (speed_span * share_of(hash, speed_shift));
    const float seconds = static_cast<float>(age) / per_second;
    return CaretSpark{toward.x * speed * seconds,
                      (toward.y * speed * seconds) + (gravity * seconds * seconds),
                      smallest + (size_span * share_of(hash, size_shift)),
                      1.0F - (static_cast<float>(age) / static_cast<float>(caret_spark_life)),
                      (hash >> tint_shift) < tinted_below};
}

CaretSparkTrack caret_sparks_followed(const CaretSparkTrack &track, std::uint32_t serial,
                                      const std::optional<CaretPoint> &origin,
                                      CaretMotionClock progress) noexcept
{
    CaretSparkTrack next = track;
    if (next.alive && progress.generation == next.generation)
    {
        next.now = std::max(next.now, progress.elapsed);
    }
    expire(next);
    next.serial = serial;
    if (!track.serial.has_value() || track.serial.value() == serial || !origin.has_value())
    {
        return next;
    }
    if (!next.alive)
    {
        ++next.generation;
        next.now = 0;
    }
    push(next, CaretSparkBurst{serial, origin.value(), next.now});
    return next;
}

CaretSparkTrack caret_sparks_paused(const CaretSparkTrack &track, std::uint32_t serial) noexcept
{
    CaretSparkTrack next = track;
    next.count = 0;
    next.alive = false;
    next.serial = serial;
    return next;
}
} // namespace nenenib::core
