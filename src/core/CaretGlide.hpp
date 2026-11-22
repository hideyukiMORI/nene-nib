#pragma once

#include "CaretMotionClock.hpp"
#include "CaretMotionTrack.hpp"
#include "CaretQuad.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// キャレットの動きの絵（尾を引く・ADR 0191・#1933。キーの移動の CaretMotion とは別）。四隅の
// 位置を決めるのはこの純関数だけで、時計は読まない（ARC-007）。経過の ms は ui が時計の port で
// 測り、合図の意図 CaretMotionTick が運ぶ。

// 動きの時間（ms・設定 `caret_motion_time`・`:set caretmotiontime=`）。既定は施主の決めの 120、
// 範囲は 40〜300、設定の画面の段の上下は 20 ずつ。
inline constexpr std::uint16_t caret_motion_time_default = 120;
inline constexpr std::uint16_t caret_motion_time_minimum = 40;
inline constexpr std::uint16_t caret_motion_time_maximum = 300;
inline constexpr std::uint16_t caret_motion_time_step = 20;

// 進む向きに近い隅から順の、時間に掛ける係数（見本の 0.45 / 0.6 / 0.85 / 1.0）。
inline constexpr std::array<float, 4> caret_corner_factors{0.45F, 0.6F, 0.85F, 1.0F};

// 時間の文字列（10 進・先頭の 0 なし・範囲の中）を読む 1 本。設定の読みと `:set` が共用する。
[[nodiscard]] std::optional<std::uint16_t> caret_motion_time_of(std::string_view text) noexcept;
// 時間を 1 段上下に送る（範囲の端で止まる・段に乗っていない値は送る向きの手前の段へ寄せる）。
[[nodiscard]] std::uint16_t caret_motion_time_stepped(std::uint16_t milliseconds,
                                                      bool forward) noexcept;
// 設定の画面に出す値の文（「120 ms」・単位は訳さない）。
[[nodiscard]] std::string caret_motion_time_text(std::uint16_t milliseconds);

// 矩形の四隅（左上・右上・右下・左下）。
[[nodiscard]] CaretQuad caret_quad_of(float left, float top, float right, float bottom) noexcept;
// 隅ごとの係数（caret_corner_factors のどれか）。to の中心から見て from → to の向きへ遠い隅ほど
// 小さい係数（先に着く）。向きが無い（中心が同じ）ときは並びの順。
[[nodiscard]] std::array<float, 4> caret_corner_shares(const CaretQuad &from,
                                                       const CaretQuad &to) noexcept;
// 動きの途中の四隅。隅 i は elapsed / (duration × 係数 i) を 0〜1 に切り詰めた進みを
// ease-out の 3 乗（1 − (1 − u)³）で写して from から to へ寄せる。duration が 0 なら to。
[[nodiscard]] CaretQuad caret_motion_quad(const CaretQuad &from, const CaretQuad &to,
                                          std::uint32_t elapsed, std::uint32_t duration) noexcept;
// 四隅を bounds（左上が corners[0]・右下が corners[2] の矩形）の中へ寄せる（画面の外から来る尾は
// 画面の端から・ADR 0191 の決定 5）。
[[nodiscard]] CaretQuad caret_quad_clamped(const CaretQuad &quad, const CaretQuad &bounds) noexcept;

// にじみの層の四隅。各隅を四隅の中心から見た側へ x と y を amount ずつ外へ広げる（平方根を使わない
// 近似・ADR 0191 の決定 4）。中心と同じ座標の向きは動かさない。
[[nodiscard]] CaretQuad caret_quad_inflated(const CaretQuad &quad, float amount) noexcept;

// 跳ぶ（動かさない）ときの覚え。target が無ければ覚えを空にする（次に見えたときも跳ぶ）。
[[nodiscard]] CaretMotionTrack
caret_motion_settled(const CaretMotionTrack &track,
                     const std::optional<CaretQuad> &target) noexcept;
// 1 frame 分の動き。行き先が前の frame と違えば、今描いている四隅（track.bounds の中へ寄せる）から
// 新しい行き先へ動きを始め直し（generation を 1 つ進め、この frame は出発の四隅）、同じなら clock
// の 番号が今の動きのときだけその経過で四隅を進める。着いたら moving を下ろす。前の行き先が無い
// （最初の frame・跳んだ後）ときと duration が 0 のときは跳ぶ。
[[nodiscard]] CaretMotionTrack caret_motion_followed(const CaretMotionTrack &track,
                                                     const CaretQuad &target,
                                                     CaretMotionClock progress,
                                                     std::uint32_t duration) noexcept;
} // namespace nenenib::core
