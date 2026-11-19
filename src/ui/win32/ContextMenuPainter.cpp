#include "ContextMenuPainter.hpp"

#include "ContextMenu.hpp"
#include "DevicePixels.hpp"
#include "PaletteRowKey.hpp"
#include "RgbaColor.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 面と同じ角丸（Ctrl+P の面 10 DIP・選んだ行 6 DIP）と、選んだ行の地の左右の内側。
constexpr float panel_radius_dips = 10.0F;
constexpr float row_radius_dips = 6.0F;
constexpr std::int32_t row_inset_dips = 4;
// 幅を測る文字組みの箱（折り返さないので幅は箱に依らない）。
constexpr std::int32_t measured_extent = 4096;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

// 選択の面は透ける色（Ctrl+P の面の選んだ行と同じ）。
[[nodiscard]] D2D1_COLOR_F to_color(core::RgbaColor color) noexcept
{
    D2D1_COLOR_F translucent = to_color(color.color);
    translucent.a = static_cast<float>(color.alpha) / full_channel;
    return translucent;
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}
} // namespace

ContextMenuPainter::ContextMenuPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                       const ContextMenuText &text,
                                       PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

float ContextMenuPainter::scaled(float dips) const noexcept
{
    return dips * static_cast<float>(text_.dpi) / static_cast<float>(core::reference_dpi);
}

ContextMenuWidths ContextMenuPainter::measure(const application::ContextMenuView &view) const
{
    ContextMenuWidths widths{0, 0};
    for (const auto &row : view.rows)
    {
        widths.name = std::max(widths.name, text_width(row.name, text_.name));
        widths.key = std::max(widths.key, text_width(row.key, text_.key));
    }
    return widths;
}

std::int32_t ContextMenuPainter::text_width(std::string_view text, IDWriteTextFormat *format) const
{
    if (text.empty())
    {
        return 0;
    }
    const auto layout = made(text, format, PaletteTextRole::plain,
                             core::LayoutRect{0, 0, measured_extent, measured_extent});
    DWRITE_TEXT_METRICS metrics{};
    if (layout == nullptr || FAILED(layout->GetMetrics(&metrics)))
    {
        return 0;
    }
    return static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace));
}

void ContextMenuPainter::paint(const application::ContextMenuView &view,
                               const core::ContextMenuLayout &layout, const core::Palette &palette)
{
    const auto panel = D2D1::RoundedRect(to_rect(layout.panel), scaled(panel_radius_dips),
                                         scaled(panel_radius_dips));
    brush_.SetColor(to_color(palette.panel));
    target_.FillRoundedRectangle(panel, &brush_);
    brush_.SetColor(to_color(palette.panel_border));
    target_.DrawRoundedRectangle(panel, &brush_, scaled(1.0F));
    for (std::size_t index = 0; index < layout.separator_count; ++index)
    {
        target_.FillRectangle(to_rect(layout.separators.at(index)), &brush_);
    }
    const auto inset = core::to_pixels(row_inset_dips, text_.dpi);
    for (std::size_t index = 0; index < view.rows.size() && index < layout.row_count; ++index)
    {
        if (view.highlighted == index)
        {
            const auto &row = layout.rows.at(index);
            const core::LayoutRect chosen{row.left + inset, row.top, row.right - inset, row.bottom};
            brush_.SetColor(to_color(palette.selection));
            target_.FillRoundedRectangle(D2D1::RoundedRect(to_rect(chosen), scaled(row_radius_dips),
                                                           scaled(row_radius_dips)),
                                         &brush_);
        }
        paint_row(view.rows.at(index), layout, index, palette);
    }
}

void ContextMenuPainter::paint_row(const application::ContextMenuRow &row,
                                   const core::ContextMenuLayout &layout, std::size_t index,
                                   const core::Palette &palette)
{
    const auto inside = core::context_menu_row_inside(layout, index);
    write(row.name, TextStyle{text_.name, PaletteTextRole::plain}, inside,
          row.enabled ? palette.text : palette.muted);
    if (!row.key.empty())
    {
        write(row.key, TextStyle{text_.key, PaletteTextRole::trailing}, inside, palette.muted);
    }
}

void ContextMenuPainter::write(std::string_view text, TextStyle style, const core::LayoutRect &area,
                               core::RgbColor color)
{
    IDWriteTextFormat *const format = style.first;
    const PaletteTextRole role = style.second;
    const auto layout = layouts_.obtain(PaletteRowKey{format, role, text, area, text_.dpi},
                                        [&] { return made(text, format, role, area); });
    if (layout == nullptr)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), layout.Get(),
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

PaletteRowLayouts::TextLayout ContextMenuPainter::made(std::string_view text,
                                                       IDWriteTextFormat *format,
                                                       PaletteTextRole role,
                                                       const core::LayoutRect &area) const
{
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    PaletteRowLayouts::TextLayout layout;
    if (text_.dwrite == nullptr || format == nullptr ||
        FAILED(text_.dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(wide.size()),
                                              format, static_cast<float>(core::width_of(area)),
                                              static_cast<float>(core::height_of(area)),
                                              layout.GetAddressOf())))
    {
        return nullptr;
    }
    switch (role)
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
