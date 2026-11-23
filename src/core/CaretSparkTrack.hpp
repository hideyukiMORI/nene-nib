#pragma once

#include "CaretSparkBurst.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 同時に生きる束の上限（直近の 8 打鍵・ADR 0192 の追補・#1979）。速い連打でも粒は 8 × 7 = 56 個を越
// えない。越えたら古い束から捨てる。
inline constexpr std::size_t caret_spark_bursts = 8;

// 打ったときの粒の覚え（ADR 0192 の追補・#1979）。描く側が 1 つ持ち、frame ごとに
// caret_sparks_followed が次の値を作る（純関数・時計は読まない）。bursts の前から count 個が生きて
// いる束（生まれた順）。serial は最後に見た打鍵の通し番号（値なしは「まだ見ていない」で、最初の
// frame は粒を出さない）。generation は粒の時計の番号（生きている束が 0 から 1 つになるたびに 1 つ
// 進む・窓はこの番号の間の経過を合図で運ぶ）、now はこの時計の今の経過（ms）、alive は生きている束
// があるか。公開 aggregate（ADR 0007）。
struct CaretSparkTrack
{
    std::array<CaretSparkBurst, caret_spark_bursts> bursts{};
    std::size_t count = 0;
    std::optional<std::uint32_t> serial = std::nullopt;
    std::uint32_t generation = 0;
    std::uint32_t now = 0;
    bool alive = false;
};
} // namespace nenenib::core
