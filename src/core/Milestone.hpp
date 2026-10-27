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
// body_presented は本文を描いた最初の frame が present された時で、起動の列の末尾（#546）。
// 問いの無い文書では最初の frame_presented と同じ frame、問いのある文書では答えの後の frame
// （ADR 0138 の決定 2）。
// device_adopted は最初の frame を WARP で子窓に出した後、本窓に本物の device で同じ frame を
// 出し終えた所（ADR 0132 の決定 3）。input_received と同じく起動の正典順の外。
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
    body_presented,
    device_adopted,
    probe_started,
    probe_finished,
    // 診断の節目（experiment/bimodal-marks・製品に入れない）。打鍵の道を割る。
    key_delivered,
    paint_entered,
    frame_built,
    draw_ended,
    latency_waited,
    presented,
    layout_created,
    // 診断の節目 2 回目: 格子の問い（ADR 0140 決定 2）の MessageBoxW の直前と返った直後。
    question_shown,
    question_answered,
    // 診断の節目 3 回目: make_layout の中。CreateTextLayout が返った直後と字体の写像の直後。
    // cell 揃えの直後は既存の layout_created と同じ点なので足さない。
    text_layout_created,
    fallback_mapped,
    // 診断の節目 4 回目: Direct2DRenderer::draw の中。Clear の後・帯・本文（と本文の案内）・
    // ステータスバー・残り（ステータスの案内と面）・EndDraw の後。
    render_cleared,
    render_title_done,
    render_body_done,
    render_status_done,
    render_other_done,
    render_draw_ended
};

// 計測 JSON に出る名前。閉じた選択肢なので既定分岐は書かない（CPP-002）。
// 診断の節目で関数長（60 行）に当たるので、case と return を 1 行に書く（experiment だけ）。
// clang-format off
[[nodiscard]] constexpr std::string_view milestone_name(Milestone milestone) noexcept
{
    switch (milestone)
    {
    case Milestone::document_opened: return "document_opened";
    case Milestone::window_created: return "window_created";
    case Milestone::backdrop_applied: return "backdrop_applied";
    case Milestone::window_shown: return "window_shown";
    case Milestone::device_created: return "device_created";
    case Milestone::swap_chain_created: return "swap_chain_created";
    case Milestone::composition_bound: return "composition_bound";
    case Milestone::context_created: return "context_created";
    case Milestone::text_formats_created: return "text_formats_created";
    case Milestone::input_received: return "input_received";
    case Milestone::frame_presented: return "frame_presented";
    case Milestone::body_presented: return "body_presented";
    case Milestone::device_adopted: return "device_adopted";
    case Milestone::probe_started: return "probe_started";
    case Milestone::probe_finished: return "probe_finished";
    case Milestone::key_delivered: return "key_delivered";
    case Milestone::paint_entered: return "paint_entered";
    case Milestone::frame_built: return "frame_built";
    case Milestone::draw_ended: return "draw_ended";
    case Milestone::latency_waited: return "latency_waited";
    case Milestone::presented: return "presented";
    case Milestone::layout_created: return "layout_created";
    case Milestone::question_shown: return "question_shown";
    case Milestone::question_answered: return "question_answered";
    case Milestone::text_layout_created: return "text_layout_created";
    case Milestone::fallback_mapped: return "fallback_mapped";
    case Milestone::render_cleared: return "render_cleared";
    case Milestone::render_title_done: return "render_title_done";
    case Milestone::render_body_done: return "render_body_done";
    case Milestone::render_status_done: return "render_status_done";
    case Milestone::render_other_done: return "render_other_done";
    case Milestone::render_draw_ended: return "render_draw_ended";
    }
    std::unreachable();
}
// clang-format on
} // namespace nenenib::core
