#include "ExplorerPainter.hpp"

#include "FilePath.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteTextRole.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 印（字体に依らない幾何学の三角・ADR 0162 の決定 2）と「…」の行（2,000 件を超えたフォルダ）。
constexpr std::string_view closed_marker = "▸";
constexpr std::string_view open_marker = "▾";
constexpr std::string_view more_mark = "…";
// ルートが無い面の先頭の行（採用案の表）。
constexpr std::string_view no_root = ":set root= でルートを決めてください";

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

// 面の index 行目（0 はルートの行）。横は面の左端から境界の手前まで。
[[nodiscard]] core::LayoutRect line_of(const core::LayoutRect &panel,
                                       const core::ExplorerLayout &layout, std::size_t index)
{
    const auto top = panel.top + (static_cast<std::int32_t>(index) * layout.line_height);
    return {panel.left, top, std::max(panel.left, panel.right - layout.border),
            top + layout.line_height};
}

[[nodiscard]] std::string_view root_name(const core::FilePath &root) noexcept
{
    const std::string_view name = root.file_name();
    return name.empty() ? root.text() : name;
}
} // namespace

ExplorerPainter::ExplorerPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                 const ExplorerText &text, PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

void ExplorerPainter::paint(const application::ExplorerView &view,
                            const core::ExplorerLayout &layout, const core::Palette &palette)
{
    if (!layout.panel.has_value())
    {
        return;
    }
    const core::LayoutRect panel = layout.panel.value();
    if (core::width_of(panel) <= 0 || core::height_of(panel) <= 0 || layout.line_height <= 0)
    {
        return;
    }
    fill(panel, palette.panel);
    target_.PushAxisAlignedClip(to_rect(panel), D2D1_ANTIALIAS_MODE_ALIASED);
    paint_root(view, line_of(panel, layout, 0), layout, palette);
    for (std::size_t index = 0; index < view.rows.size(); ++index)
    {
        const auto line = line_of(panel, layout, index + 1);
        if (line.top >= panel.bottom)
        {
            break;
        }
        if (view.selected == view.first_visible + index)
        {
            fill(line, palette.current_line);
        }
        paint_row(view.rows.at(index), line, layout, palette);
    }
    const core::LayoutRect border{std::max(panel.left, panel.right - layout.border), panel.top,
                                  panel.right, panel.bottom};
    fill(border, view.focused ? palette.accent : palette.panel_border);
    target_.PopAxisAlignedClip();
}

void ExplorerPainter::fill(const core::LayoutRect &area, core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.FillRectangle(to_rect(area), &brush_);
}

void ExplorerPainter::write(std::string_view text, const core::LayoutRect &area,
                            core::RgbColor color)
{
    const auto shown =
        layouts_.obtain(PaletteRowKey{text_.format, PaletteTextRole::plain, text, area, text_.dpi},
                        [&] { return made(text, area); });
    if (!shown)
    {
        return;
    }
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), shown.Get(),
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void ExplorerPainter::paint_root(const application::ExplorerView &view,
                                 const core::LayoutRect &line, const core::ExplorerLayout &layout,
                                 const core::Palette &palette)
{
    const core::LayoutRect area{line.left + layout.padding, line.top, line.right - layout.padding,
                                line.bottom};
    const std::string_view text = view.root.has_value() ? root_name(view.root.value()) : no_root;
    write(text, area, palette.muted);
}

void ExplorerPainter::paint_row(const application::ExplorerRowView &row,
                                const core::LayoutRect &line, const core::ExplorerLayout &layout,
                                const core::Palette &palette)
{
    // 深さは面の幅を超えて数えない（深い木で桁あふれしない）。
    const auto depth = static_cast<std::int32_t>(
        std::min<std::size_t>(row.depth, static_cast<std::size_t>(core::width_of(line))));
    const std::int32_t left =
        std::min(line.left + layout.padding + (depth * layout.indent), line.right);
    const std::int32_t right = std::max(left, line.right - layout.padding);
    const std::int32_t name_left = std::min(left + layout.marker, right);
    const core::LayoutRect name{name_left, line.top, right, line.bottom};
    if (row.more)
    {
        write(more_mark, name, palette.muted);
        return;
    }
    if (row.folder)
    {
        const core::LayoutRect marker{left, line.top, name_left, line.bottom};
        write(row.open ? open_marker : closed_marker, marker, palette.muted);
    }
    write(row.name, name, row.hidden ? palette.muted : palette.text);
}

PaletteRowLayouts::TextLayout ExplorerPainter::made(std::string_view text,
                                                    const core::LayoutRect &area)
{
    const std::wstring wide = core::to_utf16(text).value_or(std::wstring{});
    PaletteRowLayouts::TextLayout layout;
    // 折り返さず幅で切る。書式の側に NO_WRAP と文字単位の省略記号を立ててある。
    if (text_.dwrite == nullptr || text_.format == nullptr ||
        FAILED(text_.dwrite->CreateTextLayout(
            wide.c_str(), static_cast<UINT32>(wide.size()), text_.format,
            static_cast<float>(core::width_of(area)), static_cast<float>(core::height_of(area)),
            layout.GetAddressOf())))
    {
        return nullptr;
    }
    return layout;
}
} // namespace nenenib::ui::win32
