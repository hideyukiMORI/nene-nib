#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// 速さの節目の閉じた一覧（ADR 0011 の決定 1）。窓は節目を打つだけで時刻を知らない。
// 最初の frame_presented が「最初の描画」で、時刻を与えるのは adapters だけ（ARC-007）。
enum class Milestone : std::uint8_t
{
    input_received,
    frame_presented
};

// 計測 JSON に出る名前。閉じた選択肢なので既定分岐は書かない（CPP-002）。
[[nodiscard]] constexpr std::string_view milestone_name(Milestone milestone) noexcept
{
    switch (milestone)
    {
    case Milestone::input_received:
        return "input_received";
    case Milestone::frame_presented:
        return "frame_presented";
    }
    std::unreachable();
}
} // namespace nenenib::core
