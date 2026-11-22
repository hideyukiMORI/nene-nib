#pragma once

#include <cstdint>

namespace nenenib::core
{
// 動きの進み（ADR 0191 の決定 3）。generation は描く側の動きの番号（行き先が変わるたびに 1 つ
// 進む）、elapsed はその動きが始まってから ui が時計の port で測った ms。描く側は番号が今の動きと
// 同じときだけ elapsed を読み、違えば 0 と読む（古い動きの進みで新しい動きを進めない）。
// 公開 aggregate（ADR 0007）。
struct CaretMotionClock
{
    std::uint32_t generation = 0;
    std::uint32_t elapsed = 0;
};
} // namespace nenenib::core
