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
constexpr std::int32_t radius_dips = 2;
constexpr std::int32_t smallest_radius = 2;
constexpr std::int32_t largest_radius = 3;

[[nodiscard]] std::int32_t rounded(float value) noexcept
{
    return static_cast<std::int32_t>(std::floor(value + 0.5F));
}

} // namespace

D2D1_ROUNDED_RECT motion_label_badge(const DWRITE_HIT_TEST_METRICS &target,
                                     const core::MotionLabel &label, D2D1_SIZE_F written,
                                     std::uint32_t dpi) noexcept
{
    // 案 C（比べる用）: 札は跳び先の升の中。上下は行の上端と下端から 1 DIP ずつ内側、左右は物理
    // 1 画素ずつ内側。幅は升 × 印の字の数（全角の跳び先の 1 文字の札は左の 1 升だけ）。字の無い
    // 跳び先は升の幅が無いので、字の幅を 1 升とする。
    const std::int32_t edge = core::to_pixels(1, dpi);
    const auto letters = static_cast<float>(std::max<std::size_t>(label.label.size(), 1));
    const float cell =
        target.width > 0.0F
            ? target.width / static_cast<float>(std::max<std::uint8_t>(label.cells, 1))
            : written.width / letters;
    const std::int32_t left = rounded(target.left) + 1;
    const std::int32_t right = std::max(rounded(target.left + (cell * letters)) - 1, left + 1);
    const std::int32_t top = rounded(target.top) + edge;
    const std::int32_t bottom = std::max(rounded(target.top + target.height) - edge, top + 1);
    const auto radius = static_cast<float>(
        std::clamp(core::to_pixels(radius_dips, dpi), smallest_radius, largest_radius));
    return D2D1::RoundedRect(D2D1::RectF(static_cast<float>(left), static_cast<float>(top),
                                         static_cast<float>(right), static_cast<float>(bottom)),
                             radius, radius);
}

float motion_label_text_dips(float body_dips) noexcept
{
    // 案 C（比べる用）: 本文と同じ大きさから 1 pt 小さく（本文の字の行の高さは行の高さと同じで、
    // 上下 1 DIP ずつ内側の札に収まらないため）。
    constexpr float point_dips = 96.0F / 72.0F;
    return std::max(body_dips - point_dips, 1.0F);
}
} // namespace nenenib::ui::win32
