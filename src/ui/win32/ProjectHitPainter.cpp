#include "ProjectHitPainter.hpp"

#include "PaletteLayout.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteTextRole.hpp"
#include "RgbaColor.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
constexpr float full_channel = 255.0F;
// 行頭を省いた窓の印（ProjectHitRow::elided・core の 1 行の形の detail と同じ字）。
constexpr std::string_view elided_mark = "…";
// 1 行の当たりが折り返さずに分かれる矩形の数の上限（双方向の字の並びで分かれる）。
constexpr std::size_t match_runs = 8;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

// 検索の面は透ける色（ADR 0037・本文の検索の当たりと同じ）。
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

// バイト位置を DirectWrite の UTF-16 の位置へ（単位数は core の正典で測る・CPP-014）。
[[nodiscard]] UINT32 utf16_at(std::string_view text, std::size_t byte)
{
    return static_cast<UINT32>(
        core::utf16_length(text.substr(0, std::min(byte, text.size()))).value_or(0));
}
} // namespace

ProjectHitPainter::ProjectHitPainter(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                                     const ProjectHitText &text,
                                     PaletteRowLayouts &layouts) noexcept
    : target_(target), brush_(brush), text_(text), layouts_(layouts)
{
}

std::string ProjectHitPainter::shown_line(const core::ProjectHitRow &hit)
{
    return hit.elided ? std::string(elided_mark) + hit.text : hit.text;
}

std::size_t ProjectHitPainter::shown_offset(const core::ProjectHitRow &hit) noexcept
{
    return hit.elided ? elided_mark.size() : 0;
}

std::string ProjectHitPainter::shown_place(const core::ProjectHitRow &hit)
{
    return hit.path + ":" + std::to_string(hit.line.value);
}

void ProjectHitPainter::paint(const core::ProjectHitRow &hit, const core::LayoutRect &area,
                              const core::Palette &palette)
{
    paint_line(hit, core::palette_hit_line(area, text_.dpi), palette);
    const auto place_area = core::palette_hit_place(area, text_.dpi);
    const auto place = obtained(shown_place(hit), text_.place, place_area);
    if (place)
    {
        draw(*place.Get(), place_area, palette.muted);
    }
}

void ProjectHitPainter::paint_line(const core::ProjectHitRow &hit, const core::LayoutRect &area,
                                   const core::Palette &palette)
{
    const auto shown = shown_line(hit);
    const auto line = obtained(shown, text_.line, area);
    if (!line)
    {
        return;
    }
    brush_.SetColor(to_color(palette.search));
    mark_match(*line.Get(), shown, hit, area);
    draw(*line.Get(), area, palette.text);
}

void ProjectHitPainter::mark_match(IDWriteTextLayout &layout, const std::string &shown,
                                   const core::ProjectHitRow &hit, const core::LayoutRect &area)
{
    const auto offset = shown_offset(hit);
    const auto begin = utf16_at(shown, offset + hit.match_begin);
    const auto end = utf16_at(shown, offset + hit.match_end);
    std::array<DWRITE_HIT_TEST_METRICS, match_runs> runs{};
    UINT32 count = 0;
    if (end <= begin ||
        FAILED(layout.HitTestTextRange(begin, end - begin, static_cast<float>(area.left),
                                       static_cast<float>(area.top), runs.data(),
                                       static_cast<UINT32>(runs.size()), &count)))
    {
        return;
    }
    target_.PushAxisAlignedClip(to_rect(area), D2D1_ANTIALIAS_MODE_ALIASED);
    for (std::size_t index = 0; index < std::min<std::size_t>(count, runs.size()); ++index)
    {
        const auto &run = runs.at(index);
        target_.FillRectangle(
            D2D1::RectF(run.left, run.top, run.left + run.width, run.top + run.height), &brush_);
    }
    target_.PopAxisAlignedClip();
}

void ProjectHitPainter::draw(IDWriteTextLayout &layout, const core::LayoutRect &area,
                             core::RgbColor color)
{
    brush_.SetColor(to_color(color));
    target_.DrawTextLayout(
        D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)), &layout,
        &brush_, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

PaletteRowLayouts::TextLayout ProjectHitPainter::obtained(std::string_view text,
                                                          IDWriteTextFormat *format,
                                                          const core::LayoutRect &area)
{
    return layouts_.obtain(PaletteRowKey{format, PaletteTextRole::trimmed, text, area, text_.dpi},
                           [&] { return made(text, format, area); });
}

// 書式が文字単位の「…」を持つので、layout は書式のまま作る（焼き方の印は鍵を分けるためだけ）。
PaletteRowLayouts::TextLayout ProjectHitPainter::made(std::string_view text,
                                                      IDWriteTextFormat *format,
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
    return layout;
}
} // namespace nenenib::ui::win32
