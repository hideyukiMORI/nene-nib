// 打ったときの粒の 1 frame（ADR 0192 の追補・#1979）。経過を固定した覚え（core の純関数）を
// CaretSparksPainter でメモリの bitmap に描き、粒の所だけが塗られ、同じ覚えは同じ画素になることを確
// かめる。frame ごとの覚え（CaretSparksFollower）は、オフ・最初の frame・打鍵・変換中・寿命の後・窓
// が見えなくなったとき・作り直しの番号を確かめる（窓・device・時計は使わない）。
#include "CaretMotionClock.hpp"
#include "CaretPoint.hpp"
#include "CaretSparkTrack.hpp"
#include "CaretSparks.hpp"
#include "CaretSparksFollower.hpp"
#include "CaretSparksPaint.hpp"
#include "CaretSparksPainter.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

constexpr std::size_t channels = 4;
constexpr core::CaretPoint origin{100.0F, 60.0F};
constexpr std::uint32_t age = 100;

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

// 1 打鍵（通し番号 7）の束が生まれて age ms の覚え。
[[nodiscard]] core::CaretSparkTrack scattered()
{
    core::CaretSparkTrack track;
    track.bursts.at(0) = core::CaretSparkBurst{7, origin, 0};
    track.count = 1;
    track.serial = 7;
    track.generation = 1;
    track.now = age;
    track.alive = true;
    return track;
}

[[nodiscard]] Pixels painted_track(SoftwareTarget &target, const core::CaretSparkTrack &track)
{
    // 1 つ目は Ubuntu 橙（accent）、2 つ目は茄子色の側の紫（ime）。試験の中の色は palette の値の写
    // し。
    const D2D1_COLOR_F head{0xE9 / 255.0F, 0x54 / 255.0F, 0x20 / 255.0F, 1.0F};
    const D2D1_COLOR_F tint{0xD7 / 255.0F, 0xC4 / 255.0F, 0xE5 / 255.0F, 1.0F};
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::CaretSparksPainter::paint(context, *brush, track,
                                             win32::CaretSparksPaint{head, tint, 1.0F});
        });
}

void verify_sparks_pixels()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the caret sparks paint needs the software target");
        return;
    }
    const auto track = scattered();
    const Pixels pixels = painted_track(target, track);
    bool every = !pixels.empty();
    for (std::size_t index = 0; index < core::caret_spark_count; ++index)
    {
        const auto spark = core::caret_spark_at(7, index, age).value_or(core::CaretSpark{});
        every = every && brightness(pixels, origin.x + spark.x, origin.y + spark.y) > 0;
    }
    expect(every, "every spark of the burst is painted where the pure function puts it");
    expect(brightness(pixels, 600.0F, 40.0F) == 0 && brightness(pixels, origin.x, 75.0F) == 0,
           "nothing is drawn away from the sparks");
    expect(painted_track(target, track) == pixels,
           "the same keystroke and the same time paint the same pixels");
    auto later = track;
    later.now = core::caret_spark_life;
    const Pixels gone = painted_track(target, later);
    bool dark = true;
    for (std::size_t at = 0; at < gone.size(); at += channels)
    {
        dark = dark && gone.at(at) == 0 && gone.at(at + 1) == 0 && gone.at(at + 2) == 0;
    }
    expect(dark, "after the life nothing is painted");
}

// 描く側の 1 frame（入口→キャレットが見えれば中心→出口）。
void framed(win32::CaretSparksFollower &follower, std::uint32_t typed,
            core::CaretMotionClock progress, bool composing = false)
{
    follower.begin(true, composing, typed, progress);
    if (follower.on())
    {
        follower.see(origin);
    }
    follower.end();
}
} // namespace

void verify_caret_sparks_paint()
{
    verify_sparks_pixels();
    win32::CaretSparksFollower follower;
    follower.allow(true, 0);
    follower.begin(false, false, 3, {});
    expect(!follower.on() && !follower.paints(), "with the setting off the sparks read nothing");
    follower.end();
    framed(follower, 3, {});
    expect(!follower.generation().has_value(), "the first frame only remembers the keystroke");
    framed(follower, 4, {});
    expect(follower.generation() == std::optional<std::uint32_t>{1} && follower.paints(),
           "a keystroke starts spark clock 1 and paints");
    framed(follower, 4, {1, 200});
    expect(follower.generation().has_value(), "the sparks live through their clock");
    framed(follower, 4, {1, core::caret_spark_life});
    expect(!follower.generation().has_value() && !follower.paints(),
           "after the life the clock stops (the window drops the timer)");
    framed(follower, 5, {1, 0}, true);
    expect(!follower.generation().has_value(), "no sparks while the IME composes");
    framed(follower, 5, {});
    expect(!follower.generation().has_value(), "a keystroke made while composing never scatters");
    framed(follower, 6, {});
    follower.settle();
    expect(!follower.generation().has_value(), "settling drops the sparks (a hidden window)");
    win32::CaretSparksFollower still;
    still.allow(false, 0);
    framed(still, 1, {});
    framed(still, 2, {});
    expect(!still.generation().has_value(), "with Windows animations off no sparks fly");
    // 作り直し: 窓が最後に追った番号 2 を下限に、次の粒の時計は 3。
    win32::CaretSparksFollower rebuilt;
    rebuilt.allow(true, 2);
    framed(rebuilt, 1, {2, 900});
    framed(rebuilt, 2, {2, 900});
    expect(rebuilt.generation() == std::optional<std::uint32_t>{3} && rebuilt.paints(),
           "a rebuilt follower numbers its spark clock above the window's last one");
}
} // namespace nenenib::tests::ui
