#include "CompletionPainter.hpp"

#include "CompletionEntries.hpp"
#include "CompletionFace.hpp"
#include "DevicePixels.hpp"
#include "PaletteRowKey.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 面と同じ角丸（Ctrl+P の面 10 DIP・選んだ行 6 DIP）と、選んだ行の地の左右の内側。
constexpr float panel_radius_dips = 10.0F;
constexpr float row_radius_dips = 6.0F;
constexpr std::int32_t row_inset_dips = 4;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}
} // namespace

CompletionPainter::CompletionPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                     const ContextMenuText &text,
                                     PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

float CompletionPainter::scaled(float dips) const noexcept
{
    return dips * static_cast<float>(text_.dpi) / static_cast<float>(core::reference_dpi);
}

void CompletionPainter::paint(const application::CompletionView &view,
                              const core::CompletionFaceLayout &layout,
                              const core::Palette &palette)
{
    const auto panel = D2D1::RoundedRect(to_rect(layout.panel), scaled(panel_radius_dips),
                                         scaled(panel_radius_dips));
    brush_.SetColor(to_color(palette.panel));
    target_.FillRoundedRectangle(panel, &brush_);
    brush_.SetColor(to_color(palette.panel_border));
    target_.DrawRoundedRectangle(panel, &brush_, scaled(1.0F));
    const auto inset = core::to_pixels(row_inset_dips, text_.dpi);
    const std::size_t count = std::min(layout.rows, view.rows.size() - layout.first);
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::size_t shown = layout.first + index;
        if (view.selected == shown)
        {
            const auto row = core::completion_face_row(layout, index);
            const core::LayoutRect chosen{row.left + inset, row.top, row.right - inset, row.bottom};
            brush_.SetColor(to_color(palette.current_line));
            target_.FillRoundedRectangle(D2D1::RoundedRect(to_rect(chosen), scaled(row_radius_dips),
                                                           scaled(row_radius_dips)),
                                         &brush_);
            frame(chosen, palette.text);
        }
        paint_row(view.rows.at(shown), layout, index, palette);
    }
}

void CompletionPainter::frame(const core::LayoutRect &area, core::RgbColor color)
{
    const float half = scaled(1.0F) / 2.0F;
    auto box = to_rect(area);
    box.left += half;
    box.top += half;
    box.right -= half;
    box.bottom -= half;
    brush_.SetColor(to_color(color));
    target_.DrawRoundedRectangle(
        D2D1::RoundedRect(box, scaled(row_radius_dips), scaled(row_radius_dips)), &brush_,
        scaled(1.0F));
}

void CompletionPainter::paint_row(const application::CompletionRowView &row,
                                  const core::CompletionFaceLayout &layout, std::size_t index,
                                  const core::Palette &palette)
{
    write(core::completion_source_mark(row.source), TextStyle{text_.key, PaletteTextRole::plain},
          core::completion_face_mark(layout, index), palette.muted);
    write(row.label, TextStyle{text_.name, PaletteTextRole::plain},
          core::completion_face_label(layout, index), palette.text);
    if (layout.detailed && !row.detail.empty())
    {
        write(row.detail, TextStyle{text_.key, PaletteTextRole::trailing},
              core::completion_face_detail(layout, index), palette.muted);
    }
}

void CompletionPainter::write(std::string_view text, TextStyle style, const core::LayoutRect &area,
                              core::RgbColor color)
{
    const auto layout =
        layouts_.obtain(PaletteRowKey{style.first, style.second, text, area, text_.dpi},
                        [&] { return made(text, style, area); });
    if (layout == nullptr)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), layout.Get(),
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

PaletteRowLayouts::TextLayout CompletionPainter::made(std::string_view text, TextStyle style,
                                                      const core::LayoutRect &area) const
{
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    PaletteRowLayouts::TextLayout layout;
    if (text_.dwrite == nullptr || style.first == nullptr ||
        FAILED(text_.dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                              style.first, static_cast<float>(core::width_of(area)),
                                              static_cast<float>(core::height_of(area)),
                                              layout.GetAddressOf())))
    {
        return nullptr;
    }
    switch (style.second)
    {
    case PaletteTextRole::plain:
    case PaletteTextRole::trimmed:
        return layout;
    case PaletteTextRole::trailing:
        return SUCCEEDED(layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)) ? layout
                                                                                   : nullptr;
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
