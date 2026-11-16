// #1003 段 G3（ADR 0167 の決定 5・採用案 docs/design/2026-11-16-finder-look.md の「当たりの行」）:
// 全文検索の 当たりの行の 2 段の画素。WIC の bitmap（SoftwareTarget）に ProjectHitPainter
// で描き、上の段（行の 本文）の当たりの文字だけに search
// の面が敷かれること（省いた窓の「…」の後ろへずれる・長さ 0 の当たり
// には敷かない）・下の段（`相対パス:行`）に墨があり search
// の面が無いこと・はみ出しが行の内側で切れる こと・行の文字組みの保持（1 行で 2
// つ・描き直しで増えない）を、ダークとライトで確かめる。 配置は製品と同じ core::palette_hit_line /
// palette_hit_place。窓・device は作らない。字の画素は 字体に依るので、Segoe UI と Consolas
// が無い機械では測らない（QLT-013）。
#include "Appearance.hpp"
#include "DirectWriteFactory.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteLayout.hpp"
#include "PaletteRowLayouts.hpp"
#include "ProjectHitPainter.hpp"
#include "ProjectHitRow.hpp"
#include "ProjectHitText.hpp"
#include "SoftwareTarget.hpp"
#include "Utf16.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
using nenenib::ui::win32::PaletteRowLayouts;
using nenenib::ui::win32::ProjectHitPainter;
using nenenib::ui::win32::ProjectHitText;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Pixels = SoftwareTarget::Pixels;

constexpr std::uint32_t dpi = 96;
// 面の行（40 DIP）。bitmap の 80 画素の高さに収める。
constexpr core::LayoutRect row{0, 20, 640, 60};
constexpr std::size_t channels = 4;

// 製品の renderer と同じ並び（段落の中央・折り返さない・文字単位の省略記号）。
[[nodiscard]] Format format_of(IDWriteFactory2 &factory, const wchar_t *family, float size)
{
    Format format;
    Microsoft::WRL::ComPtr<IDWriteInlineObject> sign;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (FAILED(factory.CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
                                        L"", format.GetAddressOf())) ||
        FAILED(factory.CreateEllipsisTrimmingSign(format.Get(), sign.GetAddressOf())) ||
        FAILED(format->SetTrimming(&trimming, sign.Get())))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

[[nodiscard]] bool pixel_is(const Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
        channels;
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

[[nodiscard]] bool same_pixel(const Pixels &left, const Pixels &right, std::int32_t x,
                              std::int32_t y)
{
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
        channels;
    return left.size() > at + 3 && right.size() > at + 3 && left.at(at) == right.at(at) &&
           left.at(at + 1) == right.at(at + 1) && left.at(at + 2) == right.at(at + 2);
}

// 矩形の中で 2 枚の絵の違う画素（search の面は透けるので、面の無い絵と比べて面の所を見る）の数と、
// その最も左と最も右の x（無ければ -1）。
using Spread = std::array<std::int32_t, 3>;
constexpr std::size_t count_at = 0;
constexpr std::size_t left_at = 1;
constexpr std::size_t right_at = 2;

// 1 段（line の top の段）の違う画素を spread へ足す。
void spread_row(Spread &spread, const Pixels &pixels, const Pixels &unmarked,
                const core::LayoutRect &line)
{
    const auto y = line.top;
    for (auto x = line.left; x < line.right; ++x)
    {
        if (same_pixel(pixels, unmarked, x, y))
        {
            continue;
        }
        ++spread.at(count_at);
        spread.at(left_at) = spread.at(left_at) < 0 ? x : std::min(spread.at(left_at), x);
        spread.at(right_at) = std::max(spread.at(right_at), x);
    }
}

[[nodiscard]] Spread spread_of(const Pixels &pixels, const Pixels &unmarked,
                               const core::LayoutRect &area)
{
    Spread spread{0, -1, -1};
    for (auto y = area.top; y < area.bottom; ++y)
    {
        spread_row(spread, pixels, unmarked, core::LayoutRect{area.left, y, area.right, y + 1});
    }
    return spread;
}

// 地の色でない画素（字の墨と面）の数。
[[nodiscard]] std::size_t ink_in(const Pixels &pixels, const core::LayoutRect &area,
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

[[nodiscard]] core::ProjectHitRow hit_of(std::string text, std::size_t begin, std::size_t end,
                                         bool elided)
{
    return core::ProjectHitRow{"src\\sub\\main.cpp",
                               core::LineNumber{412},
                               core::Column{5},
                               std::move(text),
                               begin,
                               end,
                               elided};
}

// 面の地を塗ってから当たりの行を描く。
class HitStrip final
{
  public:
    HitStrip(SoftwareTarget &target, const ProjectHitText &text, PaletteRowLayouts &layouts,
             const core::Palette &palette)
        : target_(target), text_(text), layouts_(layouts), palette_(palette)
    {
    }
    [[nodiscard]] Pixels operator()(const core::ProjectHitRow &hit) const
    {
        return target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                const auto &panel = palette_.panel;
                brush->SetColor(D2D1::ColorF(static_cast<float>(panel.red) / 255.0F,
                                             static_cast<float>(panel.green) / 255.0F,
                                             static_cast<float>(panel.blue) / 255.0F, 1.0F));
                surface.FillRectangle(
                    D2D1::RectF(0.0F, 0.0F, SoftwareTarget::width, SoftwareTarget::height), brush);
                ProjectHitPainter painter(surface, *brush, text_, layouts_);
                painter.paint(hit, row, palette_);
            });
    }

  private:
    SoftwareTarget &target_;
    const ProjectHitText &text_;
    PaletteRowLayouts &layouts_;
    const core::Palette &palette_;
};

void verify_palette(SoftwareTarget &target, IDWriteFactory2 &factory,
                    const std::vector<Format> &formats, const core::Palette &palette)
{
    PaletteRowLayouts layouts;
    const ProjectHitText text{&factory, formats.at(0).Get(), formats.at(1).Get(), dpi};
    const HitStrip strip(target, text, layouts, palette);
    const auto line = core::palette_hit_line(row, dpi);
    const auto place = core::palette_hit_place(row, dpi);
    const auto plain = strip(hit_of("int needle = 0;", 4, 10, false));
    const auto unmarked = strip(hit_of("int needle = 0;", 4, 4, false));
    const auto marked = spread_of(plain, unmarked, line);
    expect(marked.at(count_at) > 0 && marked.at(left_at) > line.left &&
               marked.at(right_at) < line.right,
           "the hit's characters on the text line get the search face");
    expect(layouts.size() == ProjectHitPainter::layouts_per_row,
           "one hit row keeps two text layouts");
    static_cast<void>(strip(hit_of("int needle = 0;", 4, 10, false)));
    expect(layouts.size() == ProjectHitPainter::layouts_per_row,
           "painting the same hit again makes no new layout");
    expect(spread_of(plain, unmarked, place).at(count_at) == 0 &&
               ink_in(plain, place, palette.panel) > 0,
           "the place line is drawn without the search face");
    const auto elided = spread_of(strip(hit_of("int needle = 0;", 4, 10, true)),
                                  strip(hit_of("int needle = 0;", 4, 4, true)), line);
    expect(elided.at(count_at) > 0 && elided.at(left_at) > marked.at(left_at),
           "an elided window starts with the mark and moves the face right");
    expect(
        ink_in(unmarked, line, palette.panel) > 0 &&
            spread_of(unmarked, strip(hit_of("int needle = 0;", 0, 0, false)), line).at(count_at) ==
                0,
        "an empty match gets no face wherever it sits");
    const auto inside_right = line.right;
    const auto long_line = strip(hit_of(std::string(200, 'w') + " needle", 201, 207, false));
    expect(ink_in(long_line, core::LayoutRect{inside_right, row.top, row.right, row.bottom},
                  palette.panel) == 0,
           "a long text line is cut inside the row");
}
} // namespace

void verify_project_hit_paint()
{
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the project hit rows need WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI") || !installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("the project hit pixels need Segoe UI and Consolas");
        return;
    }
    const std::vector<Format> formats{format_of(*factory.Get(), L"Consolas", 12.5F),
                                      format_of(*factory.Get(), L"Segoe UI", 11.0F)};
    if (formats.at(0) == nullptr || formats.at(1) == nullptr)
    {
        unmeasured("the project hit formats could not be made");
        return;
    }
    verify_palette(target, *factory.Get(), formats, core::palette_for(core::Appearance::dark));
    verify_palette(target, *factory.Get(), formats, core::palette_for(core::Appearance::light));
}
} // namespace nenenib::tests::ui
