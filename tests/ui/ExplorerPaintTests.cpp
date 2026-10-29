// #997（ADR 0162 の決定 2・8・9）: エクスプローラーの面の画素。WIC の bitmap（SoftwareTarget）に
// ExplorerPainter で描き、地・境界（焦点で accent）・選んだ行・印・字下げ・名前の右端・面が無い
// ときの不変・行の文字組みの保持を確かめる。窓・device は作らない。字の画素は字体に依るので、
// Segoe UI が無い機械では測らない（QLT-013）。
#include "DirectWriteFactory.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerPainter.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerText.hpp"
#include "ExplorerView.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace application = nenenib::application;
using nenenib::ui::win32::ExplorerPainter;
using nenenib::ui::win32::ExplorerText;
using nenenib::ui::win32::PaletteRowLayouts;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;

// 面は bitmap の左上 240 × 80。行の高さ 20 で、ルートの行と 3 行。
constexpr core::LayoutRect panel{0, 0, 240, 80};
constexpr std::int32_t line_height = 20;
constexpr std::uint32_t dpi = 96;

[[nodiscard]] core::ExplorerLayout layout_of(bool shown)
{
    auto layout = core::explorer_layout(core::LayoutRect{0, 0, 1300, 800}, dpi,
                                        core::default_font_size(), shown);
    if (shown)
    {
        layout.panel = panel;
    }
    layout.line_height = line_height;
    return layout;
}

// 選んだ行は 2 行目の adr（木の行 1・first_visible 0）。3 行目は隠し属性の長い名前。
[[nodiscard]] application::ExplorerView view_of(bool focused)
{
    std::vector<application::ExplorerRowView> rows{
        {"docs", 0, true, true, false, false},
        {"adr", 1, true, false, false, false},
        {std::string(80, 'w') + ".md", 1, false, false, true, false},
    };
    return application::ExplorerView{core::FilePath::parse("C:/work/NeNeNib").value(),
                                     std::move(rows),
                                     3,
                                     std::size_t{1},
                                     focused,
                                     0};
}

[[nodiscard]] Format format_of(IDWriteFactory2 &factory)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0F,
                                        L"", format.GetAddressOf())))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (FAILED(factory.CreateEllipsisTrimmingSign(format.Get(), ellipsis.GetAddressOf())) ||
        FAILED(format->SetTrimming(&trimming, ellipsis.Get())))
    {
        return nullptr;
    }
    return format;
}

// 画素（BGRA・premultiplied・不透明）が色と同じか。
[[nodiscard]] bool pixel_is(const SoftwareTarget::Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) * 4;
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

// 矩形の中で色と違う画素（字の墨）の数。
[[nodiscard]] std::size_t ink_in(const SoftwareTarget::Pixels &pixels, const core::LayoutRect &area,
                                 core::RgbColor ground)
{
    std::size_t ink = 0;
    for (auto y = area.top; y < area.bottom; ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            ink += pixel_is(pixels, x, y, ground) ? 0 : 1;
        }
    }
    return ink;
}

// 面を WIC の bitmap に描く口。palette と保持を束ねて、描く面と配置だけを受ける。
[[nodiscard]] auto painter_of(SoftwareTarget &target, const ExplorerText &text,
                              PaletteRowLayouts &layouts, const core::Palette &palette)
{
    return [&target, text, &layouts, palette](const application::ExplorerView &view,
                                              const core::ExplorerLayout &layout)
    {
        return target.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                ExplorerPainter painter(surface, *brush, text, layouts);
                painter.paint(view, layout, palette);
            });
    };
}

void verify_panel_pixels(SoftwareTarget &target, IDWriteFactory2 &factory,
                         IDWriteTextFormat &format)
{
    const auto palette = core::palette_for(core::Appearance::dark);
    expect(palette.panel != palette.current_line && palette.panel != palette.panel_border &&
               palette.panel_border != palette.accent,
           "the dark palette tells the panel tokens apart");
    const auto layout = layout_of(true);
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    const auto painted = painter_of(target, ExplorerText{&factory, &format, dpi}, layouts, palette);
    const auto pixels = painted(view_of(false), layout);
    expect(pixels.size() ==
               static_cast<std::size_t>(SoftwareTarget::width) * SoftwareTarget::height * 4,
           "the explorer panel is painted");
    const auto border_x = panel.right - layout.border;
    expect(pixel_is(pixels, border_x - 2, 2, palette.panel), "the panel ground is the panel token");
    expect(pixel_is(pixels, border_x, 10, palette.panel_border) &&
               pixel_is(pixels, border_x, 70, palette.panel_border),
           "the border without focus is panel_border along the whole height");
    expect(pixel_is(pixels, border_x - 2, 50, palette.current_line) &&
               pixel_is(pixels, 1, 50, palette.current_line),
           "the selected row is current_line across the whole row");
    expect(pixel_is(pixels, border_x - 2, 30, palette.panel) &&
               pixel_is(pixels, border_x - 2, 70, palette.panel),
           "unselected rows keep the panel ground");
    expect(pixel_is(pixels, panel.right + 4, 30, core::RgbColor{0, 0, 0}),
           "the panel paints nothing right of its border");
    // 印の升は「余白 + 深さ × 字下げ」から印の幅。深さ 0 の docs と深さ 1 の adr
    // に印、ファイルに無い。
    const auto marker_at = [&](std::int32_t depth, std::int32_t row)
    {
        const auto left = panel.left + layout.padding + (depth * layout.indent);
        return core::LayoutRect{left, row * line_height, left + layout.marker,
                                (row + 1) * line_height};
    };
    expect(ink_in(pixels, marker_at(0, 1), palette.panel) > 0, "an open folder shows its marker");
    expect(ink_in(pixels, marker_at(1, 2), palette.current_line) > 0,
           "a closed folder one level down shows its marker after the indent");
    expect(ink_in(pixels,
                  core::LayoutRect{panel.left, 40, panel.left + layout.padding + layout.indent - 1,
                                   60},
                  palette.current_line) == 0,
           "the indent of the second level stays empty");
    expect(ink_in(pixels, marker_at(1, 3), palette.panel) == 0, "a file has no marker");
    expect(ink_in(pixels, core::LayoutRect{panel.left + layout.padding, 0, 120, 20},
                  palette.panel) > 0,
           "the root name is on the first line");
    // 長い名前は余白の手前で切れ、余白と境界の手前には墨が無い。
    const auto name_right = border_x - layout.padding;
    expect(ink_in(pixels, core::LayoutRect{name_right - 40, 60, name_right, 80}, palette.panel) > 0,
           "a long name runs up to the right padding");
    expect(ink_in(pixels, core::LayoutRect{name_right, 60, border_x, 80}, palette.panel) == 0,
           "a long name is cut before the right padding");
    // 字の文字組みは保持から引く（ルート・印 2 つ・名前 3 つ）。2 回目の描画で増えない。
    const auto held = layouts.size();
    expect(held == 6, "every visible field is held once");
    static_cast<void>(painted(view_of(false), layout));
    expect(layouts.size() == held, "the same panel in the next frame creates no layout");
}

void verify_focus_and_hidden(SoftwareTarget &target, IDWriteFactory2 &factory,
                             IDWriteTextFormat &format)
{
    const auto palette = core::palette_for(core::Appearance::light);
    const auto layout = layout_of(true);
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    const auto painted = painter_of(target, ExplorerText{&factory, &format, dpi}, layouts, palette);
    const auto focused = painted(view_of(true), layout);
    const auto border_x = panel.right - layout.border;
    expect(pixel_is(focused, border_x, 10, palette.accent) &&
               pixel_is(focused, border_x, 70, palette.accent),
           "the border with focus is the accent");
    // 面が無い frame は何も描かない（bitmap は黒のまま）。
    const auto hidden = painted(view_of(true), layout_of(false));
    expect(ink_in(hidden, core::LayoutRect{0, 0, 300, 80}, core::RgbColor{0, 0, 0}) == 0,
           "a frame without the panel leaves every pixel as it was");
    // ルートの無い面は案内の 1 行だけ。
    auto rootless = view_of(false);
    rootless.root = std::nullopt;
    rootless.rows.clear();
    rootless.selected = std::nullopt;
    const auto guide = painted(rootless, layout);
    expect(ink_in(guide, core::LayoutRect{panel.left + layout.padding, 0, border_x, 20},
                  palette.panel) > 0 &&
               ink_in(guide, core::LayoutRect{0, 20, border_x, 80}, palette.panel) == 0,
           "a panel without a root shows the guide line only");
}
} // namespace

void verify_explorer_paint()
{
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the explorer panel needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI"))
    {
        unmeasured("the explorer panel pixels need Segoe UI");
        return;
    }
    const auto format = format_of(*factory.Get());
    expect(format != nullptr, "the explorer name format is created");
    if (format == nullptr)
    {
        return;
    }
    verify_panel_pixels(target, *factory.Get(), *format.Get());
    verify_focus_and_hidden(target, *factory.Get(), *format.Get());
}
} // namespace nenenib::tests::ui
