// 打ったときの粒の frame ごとの覚え（ADR 0192 の追補・#1979）。時計・乱数・OS に触れない。
#include "CaretSparksFollower.hpp"

#include "CaretSparks.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
void CaretSparksFollower::allow(bool allowed, std::uint32_t floor) noexcept
{
    allowed_ = allowed;
    track_.generation = std::max(track_.generation, floor);
}

void CaretSparksFollower::begin(bool wanted, bool composing, std::uint32_t typed,
                                core::CaretMotionClock progress) noexcept
{
    advanced_ = false;
    seen_ = std::nullopt;
    wanted_ = wanted;
    if (!wanted_)
    {
        // オフの間の打鍵は数えない（番号を忘れ、次にオンになった最初の frame は覚えるだけ）。
        if (track_.alive || track_.serial.has_value())
        {
            core::CaretSparkTrack cleared{};
            cleared.generation = track_.generation;
            track_ = cleared;
        }
        return;
    }
    paused_ = composing || !allowed_;
    typed_ = typed;
    progress_ = progress;
}

bool CaretSparksFollower::on() const noexcept
{
    return wanted_ && !paused_;
}

void CaretSparksFollower::see(core::CaretPoint center) noexcept
{
    seen_ = center;
}

const core::CaretSparkTrack &CaretSparksFollower::advance() noexcept
{
    if (!wanted_ || std::exchange(advanced_, true))
    {
        return track_;
    }
    track_ = paused_ ? core::caret_sparks_paused(track_, typed_)
                     : core::caret_sparks_followed(track_, typed_, seen_, progress_);
    return track_;
}

bool CaretSparksFollower::paints() const noexcept
{
    return wanted_ && !paused_ && track_.alive;
}

void CaretSparksFollower::end() noexcept
{
    static_cast<void>(advance());
}

void CaretSparksFollower::settle() noexcept
{
    if (track_.alive)
    {
        track_ = core::caret_sparks_paused(track_, track_.serial.value_or(typed_));
    }
}

std::optional<std::uint32_t> CaretSparksFollower::generation() const noexcept
{
    if (!track_.alive)
    {
        return std::nullopt;
    }
    return track_.generation;
}
} // namespace nenenib::ui::win32
