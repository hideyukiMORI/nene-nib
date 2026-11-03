#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 診断の波線を引く帯（ADR 0161 の決定 4・物理画素）。left と right は字の run の端（小数）で、
// bottom は波の下端（下線と同じく行の下端から caret_inset 上）。amplitude（振幅）・thickness
// （太さ）・period（周期）は DPI で写した整数の画素。
struct WaveBand
{
    float left;
    float right;
    std::int32_t bottom;
    std::int32_t amplitude;
    std::int32_t thickness;
    std::int32_t period;
};
} // namespace nenenib::ui::win32
