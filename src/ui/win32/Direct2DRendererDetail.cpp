#include "Direct2DRendererDetail.hpp"

#include "BodyLayout.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "LineView.hpp"
#include "RgbaColor.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace nenenib::ui::win32::detail
{
namespace
{
constexpr float full_channel = 255.0F;
} // namespace

D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

D2D1_COLOR_F to_color(core::RgbaColor color) noexcept
{
    D2D1_COLOR_F opaque = to_color(color.color);
    opaque.a = static_cast<float>(color.alpha) / full_channel;
    return opaque;
}

D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

// 本文のキャレットを描かないとき。入力行が開いているか、エクスプローラーの面に焦点があるとき
// （採用案の表「焦点」・Ctrl+P の面と同じ）。
bool body_caret_hidden(const application::EditorFrame &frame) noexcept
{
    return frame.command_line.has_value() ||
           (frame.explorer.has_value() && frame.explorer.value().focused);
}

std::int32_t row_pixel(const core::BodyLayout &body, std::int64_t row)
{
    const auto value =
        static_cast<double>(body.content.top) + static_cast<double>(row) * body.line_height;
    return static_cast<std::int32_t>(
        std::clamp(value, static_cast<double>(std::numeric_limits<std::int32_t>::min()) / 2.0,
                   static_cast<double>(std::numeric_limits<std::int32_t>::max()) / 2.0));
}

// 行の文字組みの領域。原点のずれ（横の送りと巨大な行の窓の x）は application が行ごとに決める
// （ADR 0139 の決定 5）。行の描画と印の 1 パスが同じ 1 本を通る。
core::LayoutRect line_area(const core::BodyLayout &body, const application::LineView &line)
{
    constexpr auto reach = static_cast<float>(std::numeric_limits<std::int32_t>::max() / 2);
    const auto left = static_cast<std::int32_t>(std::clamp(line.left, -reach, reach));
    return core::LayoutRect{
        body.content.left + left, row_pixel(body, line.screen_row), body.content.right,
        row_pixel(body, line.screen_row + static_cast<std::int64_t>(line.screen_rows))};
}
} // namespace nenenib::ui::win32::detail
