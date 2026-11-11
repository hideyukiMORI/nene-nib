#include "KeycapPainter.hpp"

#include <algorithm>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;

[[nodiscard]] D2D1_COLOR_F color_of(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}
} // namespace

void paint_keycap(const KeycapCanvas &canvas, IDWriteTextLayout *shown,
                  const core::LayoutRect &note, const KeycapStyle &style)
{
    auto &[target, brush, scale] = canvas;
    DWRITE_TEXT_METRICS metrics{};
    if (core::width_of(note) <= 0 || core::height_of(note) <= 0 || shown == nullptr ||
        FAILED(shown->GetMetrics(&metrics)))
    {
        return;
    }
    const float padding = style.padding * scale;
    const float stroke = scale;
    const auto right = static_cast<float>(note.right);
    const float width =
        std::min(metrics.width + (padding * 2.0F), static_cast<float>(core::width_of(note)));
    const float middle = static_cast<float>(note.top + note.bottom) / 2.0F;
    const float half_height = style.height * scale / 2.0F;
    const D2D1_RECT_F box{right - width + (stroke / 2.0F), middle - half_height,
                          right - (stroke / 2.0F), middle + half_height};
    const float radius = style.radius * scale;
    brush.SetColor(color_of(style.face));
    target.FillRoundedRectangle(D2D1::RoundedRect(box, radius, radius), &brush);
    brush.SetColor(color_of(style.border));
    target.DrawRoundedRectangle(D2D1::RoundedRect(box, radius, radius), &brush, stroke);
    brush.SetColor(color_of(style.text));
    target.PushAxisAlignedClip(box, D2D1_ANTIALIAS_MODE_ALIASED);
    target.DrawTextLayout(D2D1::Point2F(right - width + padding, static_cast<float>(note.top)),
                          shown, &brush);
    target.PopAxisAlignedClip();
}
} // namespace nenenib::ui::win32
