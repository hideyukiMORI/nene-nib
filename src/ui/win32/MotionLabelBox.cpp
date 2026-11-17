#include "MotionLabelBox.hpp"

#include "DevicePixels.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::int32_t side_dips = 2;
constexpr std::int32_t edge_dips = 1;
constexpr std::int32_t radius_dips = 2;
constexpr std::int32_t smallest_radius = 2;
constexpr std::int32_t largest_radius = 3;
constexpr float smallest_text_dips = 10.0F;
constexpr float text_ratio = 0.6F;
// 札が升の上端より下へ掛かる量は札の高さの 1/3（残りの 2/3 は行の上へ出る）。
constexpr std::int32_t overlap_share = 3;

[[nodiscard]] std::int32_t rounded(float value) noexcept
{
    return static_cast<std::int32_t>(std::floor(value + 0.5F));
}

[[nodiscard]] std::int32_t ceiled(float value) noexcept
{
    return static_cast<std::int32_t>(std::ceil(value));
}

// 左上と字の大きさから札を作る（幅と高さは字 + 余白・半径は 2〜3 画素）。
[[nodiscard]] D2D1_ROUNDED_RECT badge_at(std::int32_t left, std::int32_t top, D2D1_SIZE_F written,
                                         std::uint32_t dpi) noexcept
{
    const std::int32_t width = ceiled(written.width) + (2 * core::to_pixels(side_dips, dpi));
    const std::int32_t height = ceiled(written.height) + (2 * core::to_pixels(edge_dips, dpi));
    const auto radius = static_cast<float>(
        std::clamp(core::to_pixels(radius_dips, dpi), smallest_radius, largest_radius));
    return D2D1::RoundedRect(D2D1::RectF(static_cast<float>(left), static_cast<float>(top),
                                         static_cast<float>(left + width),
                                         static_cast<float>(top + height)),
                             radius, radius);
}
} // namespace

D2D1_ROUNDED_RECT motion_label_badge(const DWRITE_HIT_TEST_METRICS &target, D2D1_SIZE_F written,
                                     float ceiling, std::uint32_t dpi) noexcept
{
    const std::int32_t height = ceiled(written.height) + (2 * core::to_pixels(edge_dips, dpi));
    const std::int32_t cell_top = rounded(target.top);
    std::int32_t top = cell_top - (height - (height / overlap_share));
    const std::int32_t limit = ceiled(ceiling);
    if (cell_top >= limit)
    {
        top = std::max(top, limit);
    }
    return badge_at(rounded(target.left) - core::to_pixels(side_dips, dpi), top, written, dpi);
}

D2D1_ROUNDED_RECT motion_label_badge_in_row(const DWRITE_HIT_TEST_METRICS &row, D2D1_SIZE_F written,
                                            std::uint32_t dpi) noexcept
{
    const std::int32_t height = ceiled(written.height) + (2 * core::to_pixels(edge_dips, dpi));
    const std::int32_t top = rounded(row.top) + ((rounded(row.height) - height) / 2);
    return badge_at(rounded(row.left), top, written, dpi);
}

float motion_label_text_dips(float body_dips) noexcept
{
    return std::max(body_dips * text_ratio, smallest_text_dips);
}
} // namespace nenenib::ui::win32
