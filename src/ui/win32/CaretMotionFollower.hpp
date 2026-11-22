#pragma once

#include "CaretMotionClock.hpp"
#include "CaretMotionStart.hpp"
#include "CaretMotionTrack.hpp"
#include "CaretQuad.hpp"
#include "PaneSide.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
// キャレットの動きの frame ごとの覚え（ADR 0192 の決定 2・3・5・6）。描く側（Direct2DRenderer）が
// 1 つ持ち、frame の入口の begin・キャレットを描く所の moves・frame の出口の end で進める。四隅は
// core の純関数（CaretGlide）が決め、ここはこの frame で動かすかとキャレットが見えたかだけを
// 持つ。end は frame のどの道（本文・全面のプレビュー・キャレットを描かない入力行）でも通り、
// その frame でキャレットが見えなければ覚えを捨てる（止まる・次に見えたときは跳ぶ・#1956 の M1）。
class CaretMotionFollower final
{
  public:
    // 窓が描く前に毎回置く。allowed は Windows の「アニメーション効果」、floor は窓が最後に追った
    // 動きの番号。番号を floor より下げないので、描く側を作り直しても（device lost・本物の device
    // の採用）controller に残った古い進みと次の動きの番号が当たらない（#1956 の R2）。
    void allow(bool allowed, std::uint32_t floor) noexcept;
    // frame の入口。動かさない frame（オフ・アニメーション効果がオフ・変換中）は覚えを空にする。
    // タブが前の frame と違うか、操作中の枠の側が同じまま本文の領域（bounds）が違えば、跳ぶ。
    void begin(const CaretMotionStart &start) noexcept;
    // この frame で動かすか（オフの道はこの真偽 1 つを読むだけ・決定 3）。
    [[nodiscard]] bool on() const noexcept;
    // 行き先の四隅で覚えを進め、まだ動いている（ふつうのキャレットを描かない）かを返す。
    [[nodiscard]] bool moves(const core::CaretQuad &target) noexcept;
    // この frame に多角形を描くか（動かす frame で、キャレットが見えて、まだ着いていない）。
    [[nodiscard]] bool paints() const noexcept;
    [[nodiscard]] const core::CaretMotionTrack &track() const noexcept;
    // frame の出口。動かす frame でキャレットが見えなかったら覚えを空にする。
    void end() noexcept;
    // 覚えを空にする（窓が見えなくなったとき・#1956 の R1）。次に見えた frame は跳ぶ。
    void settle() noexcept;
    // 動いていれば今の動きの番号、止まっていれば値なし（窓は描いた後に読み、動いている間だけ合図
    // を送る）。
    [[nodiscard]] std::optional<std::uint32_t> generation() const noexcept;

  private:
    core::CaretMotionTrack track_{};
    bool allowed_ = true;
    bool on_ = false;
    bool seen_ = false;
    bool jump_ = false;
    std::size_t tab_ = 0;
    core::PaneSide side_ = core::PaneSide::first;
    core::CaretMotionClock progress_{};
    std::uint32_t duration_ = 0;
};
} // namespace nenenib::ui::win32
