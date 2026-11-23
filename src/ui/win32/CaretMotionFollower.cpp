// キャレットの動きの frame ごとの覚え（ADR 0192・#1933・#1956）。時計・OS に触れない。
#include "CaretMotionFollower.hpp"

#include "CaretGlide.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
void CaretMotionFollower::allow(bool allowed, std::uint32_t floor) noexcept
{
    allowed_ = allowed;
    track_.generation = std::max(track_.generation, floor);
}

void CaretMotionFollower::begin(const CaretMotionStart &start) noexcept
{
    seen_ = false;
    on_ = start.wanted && allowed_;
    if (!on_)
    {
        settle();
        return;
    }
    // 操作中の枠が同じまま本文の領域が変わった frame（split の境界線のドラッグ・窓の大きさ・
    // エクスプローラーの開閉）も跳ぶ。枠の配置で動いた画面の位置は、キャレットが動いたのではない。
    // 操作中の枠が替わった frame は動く（決定 5）。
    jump_ = start.tab != tab_ || (start.side == side_ && start.bounds != track_.bounds);
    tab_ = start.tab;
    side_ = start.side;
    progress_ = start.progress;
    duration_ = start.duration;
    track_.bounds = start.bounds;
}

bool CaretMotionFollower::on() const noexcept
{
    return on_;
}

bool CaretMotionFollower::moves(const core::CaretQuad &target) noexcept
{
    if (!on_)
    {
        return false;
    }
    seen_ = true;
    track_ = jump_ ? core::caret_motion_settled(track_, target)
                   : core::caret_motion_followed(track_, target, progress_, duration_);
    return track_.moving;
}

bool CaretMotionFollower::paints() const noexcept
{
    return on_ && seen_ && track_.moving && track_.target.has_value();
}

const core::CaretMotionTrack &CaretMotionFollower::track() const noexcept
{
    return track_;
}

void CaretMotionFollower::end() noexcept
{
    if (on_ && !seen_)
    {
        settle();
    }
}

void CaretMotionFollower::settle() noexcept
{
    if (track_.target.has_value() || track_.moving)
    {
        track_ = core::caret_motion_settled(track_, std::nullopt);
    }
}

std::optional<std::uint32_t> CaretMotionFollower::generation() const noexcept
{
    if (!track_.moving)
    {
        return std::nullopt;
    }
    return track_.generation;
}
} // namespace nenenib::ui::win32
