#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 実験（単打の切り分け・D1）だけの印。合成ルートが adapters の記録へ結ぶ。製品の節目は増やさない。
// 点: 0x10000 | 文字 = WM_CHAR 受信、1 = controller.frame() 終わり、2 = EndDraw 終わり、
// 3 = latency の待ち終わり、4 = Present 終わり。
using ExperimentProbe = void (*)(std::uint32_t point);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline ExperimentProbe experiment_probe = nullptr;

inline void experiment_mark(std::uint32_t point)
{
    if (experiment_probe != nullptr)
    {
        experiment_probe(point);
    }
}
} // namespace nenenib::ui::win32
