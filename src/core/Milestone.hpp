#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// 速さの節目の閉じた一覧（ADR 0011 の決定 1・Issue #19 で起動の段を足した）。
// 窓は節目を打つだけで時刻を知らない。最初の frame_presented が「最初の描画」で、
// 時刻を与えるのは adapters だけ（ARC-007）。document_opened から text_formats_created までは
// 起動の経路の正典順で、各段が返った直後に 1 つずつ打つ。
enum class Milestone : std::uint8_t
{
    document_opened,
    window_created,
    backdrop_applied,
    device_created,
    swap_chain_created,
    composition_bound,
    context_created,
    text_formats_created,
    input_received,
    frame_presented
};

// 計測 JSON に出る名前。閉じた選択肢なので既定分岐は書かない（CPP-002）。
[[nodiscard]] constexpr std::string_view milestone_name(Milestone milestone) noexcept
{
    switch (milestone)
    {
    case Milestone::document_opened:
        return "document_opened";
    case Milestone::window_created:
        return "window_created";
    case Milestone::backdrop_applied:
        return "backdrop_applied";
    case Milestone::device_created:
        return "device_created";
    case Milestone::swap_chain_created:
        return "swap_chain_created";
    case Milestone::composition_bound:
        return "composition_bound";
    case Milestone::context_created:
        return "context_created";
    case Milestone::text_formats_created:
        return "text_formats_created";
    case Milestone::input_received:
        return "input_received";
    case Milestone::frame_presented:
        return "frame_presented";
    }
    std::unreachable();
}
} // namespace nenenib::core
