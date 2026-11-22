// キャレットの動きの純関数（ADR 0191・#1933）。時計・OS に触れない（ARC-007）。
#include "CaretGlide.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::size_t corner_count = 4;

[[nodiscard]] CaretPoint center_of(const CaretQuad &quad) noexcept
{
    const auto &first = quad.corners.at(0);
    const auto &third = quad.corners.at(2);
    return CaretPoint{(first.x + third.x) / 2.0F, (first.y + third.y) / 2.0F};
}

// ease-out の 3 乗（見本の式）。u は 0〜1。
[[nodiscard]] float eased(float progress) noexcept
{
    const float rest = 1.0F - progress;
    return 1.0F - (rest * rest * rest);
}

[[nodiscard]] float between(float from, float to, float share) noexcept
{
    return from + ((to - from) * share);
}

// 順位は「自分より遠い隅の数（同じなら前の隅を先）」。並べ替えの STL を使わない。
[[nodiscard]] std::size_t rank_of(const std::array<float, corner_count> &reach,
                                  std::size_t index) noexcept
{
    std::size_t rank = 0;
    for (std::size_t other = 0; other < corner_count; ++other)
    {
        if (reach.at(other) > reach.at(index) ||
            (reach.at(other) == reach.at(index) && other < index))
        {
            ++rank;
        }
    }
    return rank;
}

[[nodiscard]] CaretMotionTrack jumped(const CaretMotionTrack &track, const CaretQuad &target)
{
    CaretMotionTrack next = track;
    next.target = target;
    next.from = target;
    next.shown = target;
    next.moving = false;
    return next;
}
} // namespace

std::optional<std::uint16_t> caret_motion_time_of(std::string_view text) noexcept
{
    constexpr std::size_t digits_limit = 3;
    constexpr std::uint32_t decimal_base = 10;
    if (text.empty() || text.size() > digits_limit || (text.size() > 1 && text.front() == '0'))
    {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    for (const char digit : text)
    {
        if (digit < '0' || digit > '9')
        {
            return std::nullopt;
        }
        value = (value * decimal_base) + static_cast<std::uint32_t>(digit - '0');
    }
    if (value < caret_motion_time_minimum || value > caret_motion_time_maximum)
    {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

std::uint16_t caret_motion_time_stepped(std::uint16_t milliseconds, bool forward) noexcept
{
    const std::uint32_t clamped = std::clamp<std::uint32_t>(milliseconds, caret_motion_time_minimum,
                                                            caret_motion_time_maximum);
    // 段は最小値からの 20 刻み。段の上にない値は送る向きの手前の段へ寄せてから 1 段送る。
    const std::uint32_t offset = clamped - caret_motion_time_minimum;
    const std::uint32_t below = offset - (offset % caret_motion_time_step);
    const std::uint32_t moved =
        forward ? below + caret_motion_time_step
                : (offset % caret_motion_time_step != 0 ? below
                   : below >= caret_motion_time_step    ? below - caret_motion_time_step
                                                        : 0U);
    return static_cast<std::uint16_t>(
        std::min<std::uint32_t>(caret_motion_time_minimum + moved, caret_motion_time_maximum));
}

std::string caret_motion_time_text(std::uint16_t milliseconds)
{
    return std::to_string(milliseconds) + " ms";
}

CaretQuad caret_quad_of(float left, float top, float right, float bottom) noexcept
{
    return CaretQuad{{CaretPoint{left, top}, CaretPoint{right, top}, CaretPoint{right, bottom},
                      CaretPoint{left, bottom}}};
}

std::array<float, 4> caret_corner_shares(const CaretQuad &from, const CaretQuad &to) noexcept
{
    const CaretPoint start = center_of(from);
    const CaretPoint end = center_of(to);
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    std::array<float, corner_count> reach{};
    for (std::size_t index = 0; index < corner_count; ++index)
    {
        const auto &corner = to.corners.at(index);
        reach.at(index) = ((corner.x - end.x) * dx) + ((corner.y - end.y) * dy);
    }
    std::array<float, corner_count> shares{};
    for (std::size_t index = 0; index < corner_count; ++index)
    {
        shares.at(index) = caret_corner_factors.at(rank_of(reach, index));
    }
    return shares;
}

CaretQuad caret_motion_quad(const CaretQuad &from, const CaretQuad &to, std::uint32_t elapsed,
                            std::uint32_t duration) noexcept
{
    if (duration == 0)
    {
        return to;
    }
    const auto shares = caret_corner_shares(from, to);
    CaretQuad shown = to;
    for (std::size_t index = 0; index < corner_count; ++index)
    {
        const float span = static_cast<float>(duration) * shares.at(index);
        const float progress = std::min(1.0F, static_cast<float>(elapsed) / span);
        const float share = eased(progress);
        const auto &start = from.corners.at(index);
        const auto &end = to.corners.at(index);
        shown.corners.at(index) =
            CaretPoint{between(start.x, end.x, share), between(start.y, end.y, share)};
    }
    return shown;
}

CaretQuad caret_quad_clamped(const CaretQuad &quad, const CaretQuad &bounds) noexcept
{
    const auto &top_left = bounds.corners.at(0);
    const auto &bottom_right = bounds.corners.at(2);
    CaretQuad clamped = quad;
    for (auto &corner : clamped.corners)
    {
        corner.x = std::clamp(corner.x, top_left.x, std::max(top_left.x, bottom_right.x));
        corner.y = std::clamp(corner.y, top_left.y, std::max(top_left.y, bottom_right.y));
    }
    return clamped;
}

CaretQuad caret_quad_inflated(const CaretQuad &quad, float amount) noexcept
{
    CaretPoint middle{0.0F, 0.0F};
    for (const auto &corner : quad.corners)
    {
        middle.x += corner.x / static_cast<float>(corner_count);
        middle.y += corner.y / static_cast<float>(corner_count);
    }
    const auto away = [amount](float value, float center) noexcept
    {
        if (value < center)
        {
            return value - amount;
        }
        return value > center ? value + amount : value;
    };
    CaretQuad grown = quad;
    for (auto &corner : grown.corners)
    {
        corner = CaretPoint{away(corner.x, middle.x), away(corner.y, middle.y)};
    }
    return grown;
}

CaretMotionTrack caret_motion_settled(const CaretMotionTrack &track,
                                      const std::optional<CaretQuad> &target) noexcept
{
    if (!target.has_value())
    {
        CaretMotionTrack next = track;
        next.target = std::nullopt;
        next.moving = false;
        return next;
    }
    return jumped(track, target.value());
}

CaretMotionTrack caret_motion_followed(const CaretMotionTrack &track, const CaretQuad &target,
                                       CaretMotionClock progress, std::uint32_t duration) noexcept
{
    if (!track.target.has_value() || duration == 0)
    {
        return jumped(track, target);
    }
    CaretMotionTrack next = track;
    if (track.target.value() != target)
    {
        next.from = caret_quad_clamped(track.shown, track.bounds);
        next.target = target;
        next.shown = next.from;
        ++next.generation;
        next.moving = next.from != target;
        return next;
    }
    if (!track.moving)
    {
        next.shown = target;
        return next;
    }
    const std::uint32_t elapsed = progress.generation == track.generation ? progress.elapsed : 0;
    next.shown = caret_motion_quad(track.from, target, elapsed, duration);
    next.moving = elapsed < duration;
    if (!next.moving)
    {
        next.shown = target;
    }
    return next;
}
} // namespace nenenib::core
