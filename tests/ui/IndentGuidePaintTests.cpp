// インデントガイドの線（ADR 0193 の決定 2・5・9・#1975）。本物の DirectWrite の layout とメモリの
// bitmap で描き、線の画素の x が「仮想桁 × 空白の幅」で字の位置と合うこと（等幅と比例の字体・空白と
// Tab）、最初の画面行の高さだけに引くこと（折り返しの 2 行目に無い）、虹の巡回、強める段の太さを
// 画素で確かめる（窓・device は作らない）。字体に結果が依るので環境依存（QLT-013）。
#include "BodyTextLayouts.hpp"
#include "IndentGuidePainter.hpp"
#include "IndentGuideRow.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "DisplayForm.hpp"
#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "IndentGuide.hpp"
#include "IndentGuidePaint.hpp"
#include "IndentGuideStyle.hpp"
#include "IndentGuides.hpp"
#include "RgbColor.hpp"
#include "TextLayoutRequest.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

constexpr std::int32_t line_height = 20;
constexpr std::int32_t layout_width = 1200;
constexpr std::size_t channels = 4;
constexpr int tolerance = 3;

[[nodiscard]] std::size_t at_of(std::int32_t x, std::int32_t y)
{
    return ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
           channels;
}

[[nodiscard]] bool lit(const Pixels &pixels, std::int32_t x, std::int32_t y)
{
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(SoftwareTarget::width) ||
        y >= static_cast<std::int32_t>(SoftwareTarget::height))
    {
        return false;
    }
    const auto at = at_of(x, y);
    return pixels.at(at) != 0 || pixels.at(at + 1) != 0 || pixels.at(at + 2) != 0;
}

// 画素の色が color に近いか（BGRA）。
[[nodiscard]] bool colored(const Pixels &pixels, std::int32_t x, std::int32_t y,
                           nenenib::core::RgbColor color)
{
    const auto at = at_of(x, y);
    const auto close_to = [](BYTE got, std::uint8_t want)
    { return std::abs(static_cast<int>(got) - static_cast<int>(want)) <= tolerance; };
    return close_to(pixels.at(at), color.blue) && close_to(pixels.at(at + 1), color.green) &&
           close_to(pixels.at(at + 2), color.red);
}

[[nodiscard]] nenenib::core::IndentGuidePaint paint_of(nenenib::core::IndentGuideStyle style)
{
    const auto &theme = nenenib::core::theme_of(nenenib::core::BuiltinTheme::ubuntu_aubergine);
    return nenenib::core::indent_guide_paint(style, 4, theme.body, theme.ui);
}

[[nodiscard]] win32::IndentGuideRow row_of(nenenib::application::IndentGuides guides, float space)
{
    return win32::IndentGuideRow{guides, 0.0F, 0.0F, static_cast<float>(line_height),
                                 space,  1.0F, 2.0F};
}

[[nodiscard]] Pixels painted(SoftwareTarget &target, const win32::IndentGuideRow &row,
                             const nenenib::core::IndentGuidePaint &paint)
{
    return target.painted([&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
                          { win32::IndentGuidePainter::paint(context, brush, row, paint); });
}

// text の UTF-16 の位置 index の字の左端（layout の座標）。
[[nodiscard]] std::optional<float> character_x(IDWriteTextLayout *layout, UINT32 index)
{
    FLOAT x = 0;
    FLOAT y = 0;
    DWRITE_HIT_TEST_METRICS hit{};
    if (FAILED(layout->HitTestTextPosition(index, FALSE, &x, &y, &hit)))
    {
        return std::nullopt;
    }
    return x;
}

// 字体 family の text で、段 level の線の x が字の位置 index と合うか（1 画素以内）、線がその x に
// 光るか。
void check_alignment(SoftwareTarget &target, std::string_view family, std::string_view text,
                     UINT32 index)
{
    win32::BodyTextLayouts layouts;
    auto settings = nenenib::core::default_editor_settings();
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    settings.font_family = nenenib::core::DisplayText::parse(family).value();
    const nenenib::application::TextLayoutRequest request{
        text, settings, nenenib::core::TextViewGeometry{layout_width, line_height, 96}};
    const auto layout = layouts.get(request);
    const auto space = layouts.space_width(nenenib::core::DisplayForm::text);
    const auto expected =
        layout.has_value() ? character_x(layout.value().Get(), index) : std::nullopt;
    if (!layout.has_value() || !space.has_value() || !expected.has_value())
    {
        unmeasured("the indent guide needs a DirectWrite body layout and its space width");
        return;
    }
    const std::size_t cells = nenenib::core::indent_cells(text);
    const std::size_t column = nenenib::core::indent_cells(text.substr(0, index));
    const float guide = static_cast<float>(column) * space.value();
    expect(std::abs(guide - expected.value()) < 1.0F,
           "the guide's x (virtual column times the space width) is the character's x");
    const nenenib::application::IndentGuides guides{cells, column, cells / column};
    const auto pixels = painted(target, row_of(guides, space.value()),
                                paint_of(nenenib::core::IndentGuideStyle::plain));
    const auto x = static_cast<std::int32_t>(std::floor(guide));
    expect(lit(pixels, x, line_height / 2) && lit(pixels, 0, line_height / 2),
           "the guides light their columns");
    expect(!lit(pixels, x + 3, line_height / 2), "a guide is one pixel wide at 96 dpi");
}

void check_rows_and_rainbow(SoftwareTarget &target)
{
    constexpr float space = 10.0F;
    const auto rainbow = paint_of(nenenib::core::IndentGuideStyle::rainbow);
    const nenenib::application::IndentGuides guides{10, 2, 5, 1};
    const auto pixels = painted(target, row_of(guides, space), rainbow);
    const std::int32_t middle = line_height / 2;
    expect(!lit(pixels, 0, line_height + 5) && !lit(pixels, 20, line_height + 5),
           "the guides cover only the first screen row of a wrapped line");
    expect(colored(pixels, 0, middle, nenenib::core::indent_guide_color(rainbow, 0, false)) &&
               colored(pixels, 80, middle, nenenib::core::indent_guide_color(rainbow, 4, false)) &&
               colored(pixels, 20, middle, nenenib::core::indent_guide_color(rainbow, 1, true)),
           "the rainbow cycles by level and the strengthened level is the role color");
    expect(nenenib::core::indent_guide_color(rainbow, 0, false) ==
                   nenenib::core::indent_guide_color(rainbow, 4, false) &&
               !(nenenib::core::indent_guide_color(rainbow, 1, false) ==
                 nenenib::core::indent_guide_color(rainbow, 0, false)),
           "levels four apart share a color and neighbours do not");
    expect(lit(pixels, 21, middle) && !lit(pixels, 41, middle) && lit(pixels, 40, middle),
           "the strengthened level is two pixels wide and the others one");
}
} // namespace

void verify_indent_guide_paint()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the indent guides need the software target");
        return;
    }
    check_alignment(target, "Cascadia Code", "        x", 4);
    check_alignment(target, "Segoe UI", "        x", 4);
    check_alignment(target, "Segoe UI", "\t\tx", 1);
    check_rows_and_rainbow(target);
}
} // namespace nenenib::tests::ui
