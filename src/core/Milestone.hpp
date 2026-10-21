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
// question_shown と question_answered は表示形式の問い（ADR 0140 の決定 2）の MessageBoxW の
// 直前と返った直後。計測器は開くの値からこの間（答えを待つ時間）を引く（#689）。
// frame_built と frame_drawn は打鍵の道の input_received -> frame_presented の間の段（#844）。
// frame_built は窓が controller の frame を作り終えた直後（描く前）、frame_drawn は
// renderer の EndDraw が返った直後（latency の待ちと Present の前）。どちらも frame ごとに
// 打ち、起動の正典順の外。
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
    question_shown,
    question_answered,
    probe_started,
    probe_finished,
    frame_built,
    frame_drawn
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
    case Milestone::body_presented:
        return "body_presented";
    case Milestone::device_adopted:
        return "device_adopted";
    case Milestone::question_shown:
        return "question_shown";
    case Milestone::question_answered:
        return "question_answered";
    case Milestone::probe_started:
        return "probe_started";
    case Milestone::probe_finished:
        return "probe_finished";
    case Milestone::frame_built:
        return "frame_built";
    case Milestone::frame_drawn:
        return "frame_drawn";
    }
    std::unreachable();
}
} // namespace nenenib::core
