#include "MotionLabelBox.hpp"

#include "DevicePixels.hpp"
#include "MotionLabel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::int32_t side_dips = 2;
constexpr std::int32_t radius_dips = 2;
constexpr std::int32_t smallest_radius = 2;
constexpr std::int32_t largest_radius = 3;
constexpr float smallest_text_dips = 10.0F;
constexpr float text_ratio = 0.6F;
// 札の高さは行の高さの 6 割まで。
constexpr float height_share = 0.6F;

[[nodiscard]] std::int32_t rounded(float value) noexcept
{
    return static_cast<std::int32_t>(std::floor(value + 0.5F));
}

[[nodiscard]] std::int32_t ceiled(float value) noexcept
{
    return static_cast<std::int32_t>(std::ceil(value));
}

// 札の幅。字の幅 + 左右の余白（2 DIP まで）で、升 × 印の字の数（全角の跳び先の升は半分の幅）を
// 超えるなら余白を詰める（0 まで）。字の無い跳び先は升の幅が無いので 2 DIP の余白のまま。
[[nodiscard]] std::int32_t badge_width(const DWRITE_HIT_TEST_METRICS &target,
                                       const core::MotionLabel &label, float written,
                                       std::uint32_t dpi) noexcept
{
    const std::int32_t text = ceiled(written);
    const std::int32_t side = core::to_pixels(side_dips, dpi);
    if (target.width <= 0.0F)
    {
        return text + (2 * side);
    }
    const float cell = target.width / static_cast<float>(std::max<std::uint8_t>(label.cells, 1));
    const auto letters = static_cast<float>(std::max<std::size_t>(label.label.size(), 1));
    const auto room = static_cast<std::int32_t>(std::floor(cell * letters));
    return text + (2 * std::clamp((room - text) / 2, 0, side));
}
} // namespace

D2D1_ROUNDED_RECT motion_label_badge(const DWRITE_HIT_TEST_METRICS &target,
                                     const core::MotionLabel &label, D2D1_SIZE_F written,
                                     std::uint32_t dpi) noexcept
{
    const auto row = static_cast<std::int32_t>(std::floor(target.height * height_share));
    const std::int32_t text_height = ceiled(written.height);
    const std::int32_t height = row > 0 ? std::min(text_height, row) : text_height;
    const std::int32_t left = rounded(target.left);
    const std::int32_t top = rounded(target.top) + 1;
    const std::int32_t width = badge_width(target, label, written.width, dpi);
    const auto radius = static_cast<float>(
        std::clamp(core::to_pixels(radius_dips, dpi), smallest_radius, largest_radius));
    return D2D1::RoundedRect(D2D1::RectF(static_cast<float>(left), static_cast<float>(top),
                                         static_cast<float>(left + width),
                                         static_cast<float>(top + height)),
                             radius, radius);
}

float motion_label_text_dips(float body_dips) noexcept
{
    return std::max(body_dips * text_ratio, smallest_text_dips);
}
} // namespace nenenib::ui::win32
