#pragma once

#include "CaretPoint.hpp"

#include <cstdint>

namespace nenenib::core
{
// 1 打鍵ぶんの粒の束（ADR 0192 の追補・#1979）。serial は application が frame に載せる打鍵の通し番
// 号（粒の向き・速さ・大きさはこの番号から決まる）、origin は打った後のキャレットの中心（物理画
// 素）、born は束が生まれたときの粒の時計の経過（ms・CaretSparkTrack の now と同じ物差し）。公開
// aggregate（ADR 0007）。
struct CaretSparkBurst
{
    std::uint32_t serial = 0;
    CaretPoint origin{0.0F, 0.0F};
    std::uint32_t born = 0;
};
} // namespace nenenib::core
