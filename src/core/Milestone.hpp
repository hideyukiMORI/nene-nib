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
// window_shown は ShowWindow が返った直後＝窓が見えるまで。device の生成より前に打つ（ADR 0013）。
enum class Milestone : std::uint8_t
{
    document_opened,
    window_created,
    backdrop_applied,
    window_shown,
    device_created,
    swap_chain_created,
    composition_bound,
    context_created,
    text_formats_created,
    input_received,
    frame_presented,
    probe_started,
    probe_finished,
    // 実験（#460）: 連打の 1 打鍵の内訳。
    intent_begun,
    intent_applied,
    intent_delivered,
    layout_begun,
    layout_made,
    frame_begun,
    frame_built,
    frame_drawn,
    // 実験（長い行の probe）
    layout_converted,
    layout_created,
    glyphs_begun,
    glyphs_collected,
    fallback_hit,
    fallback_miss_begun,
    fallback_miss_end,
    fallback_keyless,
    body_draw_begun,
    glyph_run_drawn,
    body_draw_end
};

// 計測 JSON に出る名前。閉じた選択肢なので既定分岐は書かない（CPP-002）。
// NOLINTNEXTLINE(readability-function-size) 実験（長い行の probe）
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
    case Milestone::window_shown:
        return "window_shown";
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
    case Milestone::probe_started:
        return "probe_started";
    case Milestone::probe_finished:
        return "probe_finished";
    case Milestone::intent_begun:
        return "intent_begun";
    case Milestone::intent_applied:
        return "intent_applied";
    case Milestone::intent_delivered:
        return "intent_delivered";
    case Milestone::layout_begun:
        return "layout_begun";
    case Milestone::layout_made:
        return "layout_made";
    case Milestone::frame_begun:
        return "frame_begun";
    case Milestone::frame_built:
        return "frame_built";
    case Milestone::frame_drawn:
        return "frame_drawn";
    case Milestone::layout_converted:
        return "layout_converted";
    case Milestone::layout_created:
        return "layout_created";
    case Milestone::glyphs_begun:
        return "glyphs_begun";
    case Milestone::glyphs_collected:
        return "glyphs_collected";
    case Milestone::fallback_hit:
        return "fallback_hit";
    case Milestone::fallback_miss_begun:
        return "fallback_miss_begun";
    case Milestone::fallback_miss_end:
        return "fallback_miss_end";
    case Milestone::fallback_keyless:
        return "fallback_keyless";
    case Milestone::body_draw_begun:
        return "body_draw_begun";
    case Milestone::glyph_run_drawn:
        return "glyph_run_drawn";
    case Milestone::body_draw_end:
        return "body_draw_end";
    }
    std::unreachable();
}
} // namespace nenenib::core
