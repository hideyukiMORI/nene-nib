// キャレットの動きの 1 frame（ADR 0191 の決定 4・#1933）。進み具合を固定した四隅（core の純関数）を
// CaretMotionPainter でメモリの bitmap に描き、多角形の中が塗られ、尾の側が尾の色へ寄り、にじみが
// 多角形のすぐ外だけを薄く照らすことを画素で確かめる（窓・device・時計は使わない）。
#include "CaretGlide.hpp"
#include "CaretMotionPaint.hpp"
#include "CaretMotionPainter.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

constexpr std::size_t channels = 4;
constexpr std::uint32_t duration = 120;
constexpr std::uint32_t elapsed = 40;

// BGRA の 1 画素の 1 色（0 = 青・1 = 緑・2 = 赤）。範囲の外は 0。
[[nodiscard]] int channel(const Pixels &pixels, float x, float y, std::size_t color)
{
    const auto column = static_cast<std::int32_t>(std::floor(x));
    const auto row = static_cast<std::int32_t>(std::floor(y));
    if (column < 0 || row < 0 || column >= static_cast<std::int32_t>(SoftwareTarget::width) ||
        row >= static_cast<std::int32_t>(SoftwareTarget::height))
    {
        return 0;
    }
    const auto at = ((static_cast<std::size_t>(row) * SoftwareTarget::width) +
                     static_cast<std::size_t>(column)) *
                    channels;
    return pixels.at(at + color);
}

[[nodiscard]] int brightness(const Pixels &pixels, float x, float y)
{
    return channel(pixels, x, y, 0) + channel(pixels, x, y, 1) + channel(pixels, x, y, 2);
}
} // namespace

void verify_caret_motion_paint()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the caret motion paint needs the software target");
        return;
    }
    const auto from = core::caret_quad_of(20.0F, 30.0F, 30.0F, 50.0F);
    const auto to = core::caret_quad_of(600.0F, 30.0F, 610.0F, 50.0F);
    const auto shown = core::caret_motion_quad(from, to, elapsed, duration);
    // 頭は Ubuntu 橙（accent）、尾は茄子色の側の紫（ime）。試験の中の色は palette の値の写し。
    const D2D1_COLOR_F head{0xE9 / 255.0F, 0x54 / 255.0F, 0x20 / 255.0F, 1.0F};
    const D2D1_COLOR_F tail{0xD7 / 255.0F, 0xC4 / 255.0F, 0xE5 / 255.0F, 1.0F};
    bool painted = false;
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *)
        {
            painted = win32::CaretMotionPainter::paint(
                context, win32::CaretMotionPaint{shown, from, to, head, tail, 6.0F});
        });
    expect(painted && !pixels.empty(), "the moving caret is painted without a failure");
    // 縦の真ん中の高さでの左右の辺（隅ごとに着く時間が違うので辺は斜め）。
    const float left = (shown.corners.at(0).x + shown.corners.at(3).x) / 2.0F;
    const float right = (shown.corners.at(1).x + shown.corners.at(2).x) / 2.0F;
    expect(right - left > 50.0F, "the frame in the middle is stretched into a trail");
    const float middle_y = 40.0F;
    expect(brightness(pixels, (left + right) / 2.0F, middle_y) > 200,
           "the inside of the trail is filled");
    const int tail_blue = channel(pixels, left + 3.0F, middle_y, 0);
    const int head_blue = channel(pixels, right - 3.0F, middle_y, 0);
    expect(tail_blue > head_blue + 20 && channel(pixels, right - 3.0F, middle_y, 2) >
                                             channel(pixels, right - 3.0F, middle_y, 0),
           "the tail leans to the tail colour and the head is the caret colour");
    const int halo = brightness(pixels, (left + right) / 2.0F, 27.0F);
    expect(halo > 0 && halo < brightness(pixels, (left + right) / 2.0F, middle_y),
           "the glow lights just outside the trail more faintly than the trail");
    expect(brightness(pixels, (left + right) / 2.0F, 10.0F) == 0 &&
               brightness(pixels, left - 30.0F, middle_y) == 0 &&
               brightness(pixels, 640.0F, middle_y) == 0,
           "nothing is drawn away from the trail");
}
} // namespace nenenib::tests::ui
