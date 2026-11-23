#pragma once

#include "CaretMotionClock.hpp"
#include "CaretPoint.hpp"
#include "CaretSpark.hpp"
#include "CaretSparkTrack.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 打ったときの粒（ADR 0192 の追補・#1979・見本 caret-motion.html の sparks）。粒の向き・速さ・大き
// さ・色は打鍵の通し番号と粒の番号から整数のハッシュで決める（乱数を読まない・ARC-007）。経過の ms
// は ui が時計の port で測り、キャレットの動きと同じ合図 CaretMotionTick が運ぶ。同じ通し番号と同じ
// 経過なら同じ粒。

// 1 打鍵で散る粒の数と、粒の寿命（ms・見本の 7 個と 420 ms）。
inline constexpr std::size_t caret_spark_count = 7;
inline constexpr std::uint32_t caret_spark_life = 420;

// serial 番目の打鍵の index 番目の粒の、生まれてから age ms の姿。寿命（caret_spark_life）の後は値
// なし。向きは真上から左右へ 1.1 rad の扇の中、速さは毎秒 40〜130 DIP、下向きに毎秒毎秒 260 DIP で
// 落ち、一辺は 2〜4.5 DIP、不透明度は寿命の間に 1 から 0 へ線形に減る。8 個に 3 個ほどが 2 つ目の
// 色。三角関数を使わない（扇は表の線形補間・core から libm のシンボルを出さない・ARC-003）。
[[nodiscard]] std::optional<CaretSpark> caret_spark_at(std::uint32_t serial, std::size_t index,
                                                       std::uint32_t age) noexcept;

// 1 frame 分の粒の覚え。progress の番号が今の粒の時計なら経過を進め、寿命の過ぎた束を捨てる。打鍵の
// 通し番号 serial が前に見た値と違い、この frame でキャレットが見えた（origin がある）なら、その中
// 心に新しい束を足す（生きている束が無ければ粒の時計の番号を 1 つ進めて 0 から数え直す・上限
// caret_spark_bursts を越えたら古い束を捨てる）。前に見た値が無い最初の frame は番号を覚えるだけ。
// 生きている束が無くなったら alive を下ろす（描き直しの合図が止まる）。
[[nodiscard]] CaretSparkTrack caret_sparks_followed(const CaretSparkTrack &track,
                                                    std::uint32_t serial,
                                                    const std::optional<CaretPoint> &origin,
                                                    CaretMotionClock progress) noexcept;

// 粒を出さない frame（アニメーション効果がオフ・IME の変換中）の覚え。生きている束を捨て、通し番号
// だけを覚える（止めている間の打鍵は後から粒にならない）。
[[nodiscard]] CaretSparkTrack caret_sparks_paused(const CaretSparkTrack &track,
                                                  std::uint32_t serial) noexcept;
} // namespace nenenib::core
