#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 群の範囲を引く道の探索（VimPatternCapture）の積みの 1 段。index の原子を at で試している所で、
// phase は次に試す枝（0 は入った所・1 は `*` の 0 回の枝か済み・2 は済み）。値は単独で妥当なので
// 公開 aggregate（CPP-003）。
struct VimPatternCaptureFrame
{
    std::size_t at;
    std::uint32_t index;
    std::uint8_t phase;
};
} // namespace nenenib::core
