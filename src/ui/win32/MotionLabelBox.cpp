#include "MotionLabelBox.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace nenenib::ui::win32
{
D2D1_RECT_F motion_label_box(const DWRITE_HIT_TEST_METRICS &target, const core::MotionLabel &label,
                             float written) noexcept
{
    const float cells = static_cast<float>(std::max<std::uint8_t>(label.cells, 1));
    const float letters = static_cast<float>(std::max<std::size_t>(label.label.size(), 1));
    const float cell = target.width > 0.0F ? target.width / cells : written / letters;
    const float spread = std::max(cells, static_cast<float>(label.label.size()));
    return D2D1::RectF(target.left, target.top, target.left + (cell * spread),
                       target.top + target.height);
}
} // namespace nenenib::ui::win32
