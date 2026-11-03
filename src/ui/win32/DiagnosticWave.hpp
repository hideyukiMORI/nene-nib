#pragma once

#include "WaveBand.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// 画素の列 x の波の高さ（帯の上端からの画素）。周期 period の三角波で、山から谷までは振幅の 2 倍。
// 位相は x の絶対位置で決めるので、隣り合う区間・行をまたいでも波がつながる。
[[nodiscard]] constexpr std::int32_t wave_offset(std::int32_t x, std::int32_t period,
                                                 std::int32_t amplitude) noexcept
{
    const std::int32_t half = std::max(period / 2, 1);
    const std::int32_t phase = ((x % period) + period) % period;
    const std::int32_t distance = std::min(phase <= half ? phase : period - phase, half);
    return distance * 2 * amplitude / half;
}

// 診断の波線の唯一の描き方（ADR 0161 の決定 4）。画素の列ごとに太さ thickness の矩形を 1 つ塗り、
// 同じ高さが続く列は 1 つの矩形にまとめる。座標は整数の画素なので、波の色は塗った色のまま
// （中間色が出ない）。Target は ID2D1RenderTarget の FillRectangle を持つ描画先（格子の替え玉も
// 同じ）。色は呼び手が brush に置く。
template <typename Target>
void draw_diagnostic_wave(Target &target, ID2D1Brush *brush, const WaveBand &band)
{
    const auto from = static_cast<std::int32_t>(std::floor(band.left));
    const auto to = static_cast<std::int32_t>(std::ceil(band.right));
    const std::int32_t period = std::max(band.period, 2);
    const std::int32_t top = band.bottom - (2 * band.amplitude) - band.thickness;
    for (std::int32_t x = from; x < to;)
    {
        const std::int32_t offset = wave_offset(x, period, band.amplitude);
        std::int32_t end = x + 1;
        while (end < to && wave_offset(end, period, band.amplitude) == offset)
        {
            ++end;
        }
        target.FillRectangle(D2D1::RectF(static_cast<float>(x), static_cast<float>(top + offset),
                                         static_cast<float>(end),
                                         static_cast<float>(top + offset + band.thickness)),
                             brush);
        x = end;
    }
}
} // namespace nenenib::ui::win32
