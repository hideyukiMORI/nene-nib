// プレビューのリンクの札の描画と当たり（施主決定 D106・#1869 の段 L2）。renderer と同じ
// LinkCardFormats・link_card_paint・paint_link_card を、本物の DirectWrite とメモリの bitmap で
// 通し、札がリンクのすぐ下（入らなければ上）に幅 400 DIP までで置かれること・ボタンが右寄せで
// 左が secondary・右が primary であること・当たりが描いた矩形と同じであること、地を panel・縁を
// panel_border・題を title の色・右のボタンを accent で塗ることを画素で確かめる（窓・device は作ら
// ない）。bitmap は高さ 80 なので、札の下半分は描く先をずらして見る。字体に結果が依るので環境依存
// （QLT-013）。
#include "DirectWriteFactory.hpp"
#include "LinkCardContent.hpp"
#include "LinkCardFormats.hpp"
#include "LinkCardInks.hpp"
#include "LinkCardLayout.hpp"
#include "LinkCardPaint.hpp"
#include "LinkCardPlace.hpp"
#include "LinkCardTarget.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "Appearance.hpp"
#include "DisplayLanguage.hpp"
#include "LayoutRect.hpp"
#include "LinkCard.hpp"
#include "LinkDestination.hpp"
#include "LinkKind.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

constexpr std::size_t channels = 4;

[[nodiscard]] std::uint32_t pixel_at(const Pixels &pixels, std::int32_t x, std::int32_t y)
{
    if (pixels.empty() || x < 0 || y < 0 || x >= static_cast<std::int32_t>(SoftwareTarget::width) ||
        y >= static_cast<std::int32_t>(SoftwareTarget::height))
    {
        return 0;
    }
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
        channels;
    return static_cast<std::uint32_t>(pixels.at(at)) |
           (static_cast<std::uint32_t>(pixels.at(at + 1)) << 8U) |
           (static_cast<std::uint32_t>(pixels.at(at + 2)) << 16U);
}

[[nodiscard]] bool near_color(std::uint32_t pixel, core::RgbColor color) noexcept
{
    const auto within = [](std::uint32_t value, std::uint8_t expected)
    { return std::abs(static_cast<int>(value & 0xFFU) - static_cast<int>(expected)) <= 2; };
    return within(pixel, color.blue) && within(pixel >> 8U, color.green) &&
           within(pixel >> 16U, color.red);
}

[[nodiscard]] D2D1_COLOR_F color_of(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / 255.0F,
                        static_cast<float>(color.green) / 255.0F,
                        static_cast<float>(color.blue) / 255.0F);
}

// 試験の色（トークンから。題と右のボタンは見分けられる色）。
[[nodiscard]] win32::LinkCardInks inks_of(const core::Palette &palette)
{
    return win32::LinkCardInks{color_of(palette.panel),     color_of(palette.panel_border),
                               color_of(palette.text),      color_of(palette.muted),
                               color_of(palette.text),      color_of(palette.accent),
                               color_of(palette.background)};
}

[[nodiscard]] win32::LinkCardContent web_content(const core::Palette &palette)
{
    const core::LinkDestination destination{core::LinkKind::web, "https://www.vim.org/docs.php"};
    return win32::LinkCardContent{
        core::link_card_texts(core::LinkKind::web, core::DisplayLanguage::japanese),
        destination.text, core::link_card_emphasis(destination), inks_of(palette)};
}

[[nodiscard]] std::size_t inked(const Pixels &pixels, const core::LayoutRect &region,
                                core::RgbColor ink)
{
    std::size_t count = 0;
    for (std::int32_t y = region.top; y < region.bottom; ++y)
    {
        for (std::int32_t x = region.left; x < region.right; ++x)
        {
            count += near_color(pixel_at(pixels, x, y), ink) ? 1U : 0U;
        }
    }
    return count;
}

// 置き場・ボタン・当たり（画素を見ない）。
void check_geometry(IDWriteFactory &factory, const win32::LinkCardFormats &formats,
                    const win32::LinkCardContent &content)
{
    const core::LayoutRect area{0, 0, 1200, 700};
    const auto below = win32::link_card_paint(factory, formats, content,
                                              win32::LinkCardPlace{{300, 100, 380, 120}, area, 96});
    expect(below.has_value() && std::abs(below.value().box.top - 124.0F) < 0.01F &&
               std::abs(below.value().box.left - 300.0F) < 0.01F &&
               below.value().box.right - below.value().box.left <= 400.0F,
           "the card sits just below the link and is at most 400 wide");
    if (!below.has_value())
    {
        return;
    }
    const auto &paint = below.value();
    expect(paint.secondary.right <= paint.primary.left && paint.primary.right <= paint.box.right &&
               paint.primary.bottom <= paint.box.bottom && paint.target_box.top > paint.box.top,
           "the buttons sit at the right with the secondary on the left");
    const auto centre_x = static_cast<std::int32_t>((paint.primary.left + paint.primary.right) / 2);
    const auto centre_y = static_cast<std::int32_t>((paint.primary.top + paint.primary.bottom) / 2);
    expect(win32::link_card_target_of(paint, centre_x, centre_y) ==
                   win32::LinkCardTarget::primary &&
               win32::link_card_target_of(
                   paint,
                   static_cast<std::int32_t>((paint.secondary.left + paint.secondary.right) / 2),
                   centre_y) == win32::LinkCardTarget::secondary &&
               win32::link_card_target_of(paint, static_cast<std::int32_t>(paint.box.left) + 3,
                                          static_cast<std::int32_t>(paint.box.top) + 3) ==
                   win32::LinkCardTarget::card &&
               win32::link_card_target_of(paint, 5, 5) == win32::LinkCardTarget::outside,
           "the hit test reads the drawn rectangles");
    const auto above = win32::link_card_paint(factory, formats, content,
                                              win32::LinkCardPlace{{300, 660, 380, 680}, area, 96});
    expect(above.has_value() && std::abs(above.value().box.bottom - 656.0F) < 0.01F,
           "the card goes above the link when there is no room below");
}

// 札の上 80 画素（shift = 0）と次の 80 画素（shift = 80）を描く。
[[nodiscard]] Pixels painted_band(SoftwareTarget &target, const win32::LinkCardPaint &paint,
                                  float shift)
{
    return target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            context.SetTransform(D2D1::Matrix3x2F::Translation(0.0F, -shift));
            win32::paint_link_card(context, *brush, paint);
            context.SetTransform(D2D1::Matrix3x2F::Identity());
        });
}

void check_pixels(SoftwareTarget &target, const win32::LinkCardPaint &paint,
                  const core::Palette &palette, const std::string &named)
{
    const auto left = static_cast<std::int32_t>(paint.box.left);
    const auto right = static_cast<std::int32_t>(paint.box.right);
    const Pixels upper = painted_band(target, paint, 0.0F);
    expect(near_color(pixel_at(upper, left + 6, 8), palette.panel),
           ("the card ground is the panel color" + named).c_str());
    expect(near_color(pixel_at(upper, left, 40), palette.panel_border),
           ("the card edge is the panel border color" + named).c_str());
    const core::LayoutRect title{left + 16, static_cast<std::int32_t>(paint.box.top) + 12,
                                 right - 16, static_cast<std::int32_t>(paint.target_box.top) - 2};
    expect(inked(upper, title, palette.text) > 0,
           ("the title is written in the title color" + named).c_str());
    expect(pixel_at(upper, left - 3, 40) == 0 && pixel_at(upper, right + 3, 40) == 0,
           ("nothing is drawn outside the card" + named).c_str());
    const Pixels lower = painted_band(target, paint, 80.0F);
    const auto primary_x = static_cast<std::int32_t>(paint.primary.left) + 4;
    const auto primary_y = static_cast<std::int32_t>(paint.primary.top + 4.0F) - 80;
    expect(near_color(pixel_at(lower, primary_x, primary_y), palette.accent),
           ("the right button is filled with the accent color" + named).c_str());
}

void check_theme(SoftwareTarget &target, IDWriteFactory &factory,
                 const win32::LinkCardFormats &formats, core::Appearance appearance)
{
    const auto palette = core::palette_for(appearance);
    const auto content = web_content(palette);
    const auto paint =
        win32::link_card_paint(factory, formats, content,
                               win32::LinkCardPlace{{40, -24, 120, -4}, {0, 0, 1300, 1000}, 96});
    if (!paint.has_value() || paint.value().primary.bottom > 160.0F)
    {
        unmeasured("the link card paint needs DirectWrite layouts that fit two bands");
        return;
    }
    check_pixels(target, paint.value(), palette,
                 appearance == core::Appearance::dark ? " (dark)" : " (light)");
}
} // namespace

void verify_link_card_paint()
{
    SoftwareTarget target;
    auto factory = directwrite_factory();
    win32::LinkCardFormats formats;
    if (!target.ready() || !factory ||
        !formats.made(*factory.Get(), {L"Segoe UI", L"Consolas"}, 1.0F, L"ja-JP"))
    {
        unmeasured("the link card paint needs the software target and DirectWrite");
        return;
    }
    check_geometry(*factory.Get(), formats, web_content(core::palette_for(core::Appearance::dark)));
    check_theme(target, *factory.Get(), formats, core::Appearance::dark);
    check_theme(target, *factory.Get(), formats, core::Appearance::light);
}
} // namespace nenenib::tests::ui
