// タブの札の描画（#1789・ADR 0056 の #1789 の追補）。renderer と同じ TabTipLayout と paint_tab_tip
// を、本物の DirectWrite とメモリの bitmap で通し、長いパスが幅の上限に収まる最も少ない切り方で
// 真ん中を「…」に替えること（短いパスは切らない・同じ鍵は作り直さない）と、地を panel・縁を
// panel_border・文字を text で札の中だけに描くこと（ダークとライトの配色）を画素で確かめる（窓・
// device は作らない）。字体に結果が依るので環境依存（QLT-013）。
#include "DirectWriteFactory.hpp"
#include "SoftwareTarget.hpp"
#include "TabTipBox.hpp"
#include "TabTipLayout.hpp"
#include "TabTipPaint.hpp"
#include "WindowChecks.hpp"

#include "Appearance.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"
#include "TabTip.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarLayout.hpp"
#include "Utf16.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;
using TextFormat = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

constexpr std::size_t channels = 4;
constexpr float ui_text_dips = 12.0F;
// 文の幅の上限（物理画素・96 DPI）。下の長いパスは収まらない。
constexpr std::int32_t limit = 300;

[[nodiscard]] D2D1_COLOR_F color_of(core::RgbColor color) noexcept
{
    constexpr float full = 255.0F;
    return D2D1::ColorF(static_cast<float>(color.red) / full,
                        static_cast<float>(color.green) / full,
                        static_cast<float>(color.blue) / full);
}

// 画素の BGRA（範囲の外は黒）。
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

// 色の BGRA と画素が各チャネル 2 以内で同じか。
[[nodiscard]] bool near_color(std::uint32_t pixel, core::RgbColor color) noexcept
{
    const auto within = [](std::uint32_t value, std::uint8_t expected)
    { return std::abs(static_cast<int>(value & 0xFFU) - static_cast<int>(expected)) <= 2; };
    return within(pixel, color.blue) && within(pixel >> 8U, color.green) &&
           within(pixel >> 16U, color.red);
}

[[nodiscard]] TextFormat tip_format(IDWriteFactory &factory)
{
    TextFormat format;
    if (FAILED(factory.CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        ui_text_dips, L"ja-JP", &format)) ||
        FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
    {
        return nullptr;
    }
    return format;
}

[[nodiscard]] float width_of(IDWriteTextLayout *layout)
{
    DWRITE_TEXT_METRICS metrics{};
    if (layout == nullptr || FAILED(layout->GetMetrics(&metrics)))
    {
        return -1.0F;
    }
    return metrics.widthIncludingTrailingWhitespace;
}

[[nodiscard]] float measured(IDWriteFactory &factory, IDWriteTextFormat *format,
                             std::string_view text)
{
    const auto wide = core::to_utf16(text);
    TextLayout layout;
    if (!wide.has_value() || FAILED(factory.CreateTextLayout(
                                 wide.value().c_str(), static_cast<UINT32>(wide.value().size()),
                                 format, 100'000.0F, 100'000.0F, &layout)))
    {
        return -1.0F;
    }
    return width_of(layout.Get());
}

// 上限に収まる最も少ない落とす数（1 つずつ数えた答え）。
[[nodiscard]] std::size_t fewest_dropped(IDWriteFactory &factory, IDWriteTextFormat *format,
                                         std::string_view text)
{
    const std::size_t total = core::tab_tip_characters(text);
    for (std::size_t dropped = 1; dropped < total; ++dropped)
    {
        const float width = measured(factory, format, core::middle_clipped(text, dropped));
        if (width >= 0.0F && width <= static_cast<float>(limit))
        {
            return dropped;
        }
    }
    return total;
}

constexpr std::string_view long_path =
    "C:\\Users\\someone\\Documents\\projects\\nene-nib\\docs\\adr\\"
    "deeply\\nested\\folder\\structure\\for\\testing\\notes.txt";

// 長いパスは上限に収まる最も少ない切り方で、短いパスは切らない。同じ鍵は同じ layout を返す。
void check_fitting(IDWriteFactory &factory, IDWriteTextFormat *format)
{
    win32::TabTipLayout held;
    IDWriteTextLayout *fitted = held.fitted(factory, format, long_path, limit);
    const std::size_t dropped = fewest_dropped(factory, format, long_path);
    const float expected = measured(factory, format, core::middle_clipped(long_path, dropped));
    expect(fitted != nullptr && width_of(fitted) <= static_cast<float>(limit),
           "a long path is narrowed within the limit");
    expect(dropped > 1 && std::abs(width_of(fitted) - expected) < 0.01F,
           "the narrowed path drops the fewest characters that fit");
    expect(held.fitted(factory, format, long_path, limit) == fitted,
           "the same text, limit and format reuse the held layout");
    const auto band = core::title_bar_layout(core::TitleBarInput{1200, 96, 3, 0, 0, std::nullopt});
    const auto paint = held.painted(band, 1, core::palette_for(core::Appearance::dark), 96);
    expect(paint.has_value() && paint.value().written == fitted &&
               std::abs(paint.value().box.left - static_cast<float>(core::tab_rect(band, 1).left)) <
                   0.01F &&
               std::abs(paint.value().box.top - static_cast<float>(band.band.bottom + 4)) < 0.01F &&
               std::abs(paint.value().origin.x - paint.value().box.left - 10.0F) < 0.01F,
           "the tip is placed under the tab's left edge with the text inside the padding");
    constexpr std::string_view short_path = "C:\\work\\a.txt";
    IDWriteTextLayout *whole = held.fitted(factory, format, short_path, limit);
    expect(whole != nullptr &&
               std::abs(width_of(whole) - measured(factory, format, short_path)) < 0.01F,
           "a short path is not clipped");
}

// 札を描き、地・縁・文字の画素と札の外を見る。
void check_paint(SoftwareTarget &target, IDWriteTextLayout *written, const core::Palette &palette,
                 const char *theme)
{
    DWRITE_TEXT_METRICS metrics{};
    static_cast<void>(written->GetMetrics(&metrics));
    const float left = 40.0F;
    const float top = 10.0F;
    const D2D1_RECT_F box = D2D1::RectF(left, top, left + std::ceil(metrics.width) + 20.0F,
                                        top + std::ceil(metrics.height) + 12.0F);
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::paint_tab_tip(
                context, brush,
                win32::TabTipPaint{box, written, D2D1::Point2F(left + 10.0F, top + 6.0F), 6.0F,
                                   1.0F, color_of(palette.panel), color_of(palette.panel_border),
                                   color_of(palette.text)});
        });
    const std::string named = std::string(" (") + theme + ")";
    const auto middle = static_cast<std::int32_t>((box.top + box.bottom) / 2.0F);
    expect(near_color(pixel_at(pixels, static_cast<std::int32_t>(left) + 4, middle), palette.panel),
           ("the tip ground is the panel color" + named).c_str());
    expect(
        near_color(pixel_at(pixels, static_cast<std::int32_t>(left), middle), palette.panel_border),
        ("the tip edge is the panel border color" + named).c_str());
    std::size_t inked = 0;
    for (auto x = static_cast<std::int32_t>(left) + 10;
         x < static_cast<std::int32_t>(box.right) - 10; ++x)
    {
        inked += near_color(pixel_at(pixels, x, middle), palette.text) ? 1U : 0U;
    }
    expect(inked > 0, ("the path is written in the text color" + named).c_str());
    expect(pixel_at(pixels, static_cast<std::int32_t>(left) - 3, middle) == 0 &&
               pixel_at(pixels, static_cast<std::int32_t>(box.right) + 3, middle) == 0 &&
               pixel_at(pixels, static_cast<std::int32_t>(left) + 20,
                        static_cast<std::int32_t>(box.bottom) + 3) == 0,
           ("nothing is drawn outside the tip" + named).c_str());
}
} // namespace

void verify_tab_tip_paint()
{
    SoftwareTarget target;
    auto factory = directwrite_factory();
    if (!target.ready() || !factory)
    {
        unmeasured("the tab tip paint needs the software target and DirectWrite");
        return;
    }
    const TextFormat format = tip_format(*factory.Get());
    if (!format)
    {
        unmeasured("the tab tip paint needs the UI text format");
        return;
    }
    check_fitting(*factory.Get(), format.Get());
    win32::TabTipLayout held;
    IDWriteTextLayout *written = held.fitted(*factory.Get(), format.Get(), long_path, limit);
    if (written == nullptr)
    {
        unmeasured("the tab tip paint needs a DirectWrite layout");
        return;
    }
    check_paint(target, written, core::palette_for(core::Appearance::dark), "dark");
    check_paint(target, written, core::palette_for(core::Appearance::light), "light");
}
} // namespace nenenib::tests::ui
