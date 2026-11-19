#pragma once

#include <cstdint>

namespace nenenib::core
{
// 表の横の送りの棒の上の点の場所（#1883）。before と after は溝のつまみの前と後ろ（押すと 1 枠
// ぶん送る）、thumb はつまみ（押すとつかむ）。
enum class PreviewTableBarPart : std::uint8_t
{
    before,
    thumb,
    after
};
} // namespace nenenib::core
