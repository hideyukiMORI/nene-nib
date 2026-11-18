// hover の札の描画（ADR 0190 の決定 6・#1834）。renderer と同じ HoverTipLayout と paint_tab_tip
// （タブの札と同じ描く 1 本）を、本物の DirectWrite とメモリの bitmap で通し、長い文が幅の上限で
// 折り返すこと・画面行が 12 を越えたら 12 行に収めること（同じ鍵は作り直さない）と、キャレットの
// 上に置くこと、地を panel・縁を panel_border・文字を text で札の中だけに 2 行描くこと（ダークと
// ライトの配色）を画素で確かめる（窓・device
// は作らない）。字体に結果が依るので環境依存（QLT-013）。
#include "DirectWriteFactory.hpp"
#include "HoverTipLayout.hpp"
#include "SoftwareTarget.hpp"
#include "TabTipBox.hpp"
#include "TabTipPaint.hpp"
#include "WindowChecks.hpp"

#include "Appearance.hpp"
#include "HoverTip.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"

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

constexpr std::size_t channels = 4;
constexpr float ui_text_dips = 12.0F;
// 文の幅の上限（物理画素・96 DPI）。下の長い文は 1 行に収まらない。
constexpr std::int32_t limit = 200;

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

// renderer の札の書式と同じ形（UI の字・折り返さない書式。折り返しは layout が決める）。
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

[[nodiscard]] DWRITE_TEXT_METRICS metrics_of(IDWriteTextLayout *layout)
{
    DWRITE_TEXT_METRICS metrics{};
    if (layout != nullptr)
    {
        static_cast<void>(layout->GetMetrics(&metrics));
    }
    return metrics;
}

constexpr std::string_view two_rows = "int main(int argc, char **argv) and one more line";

// 長い文は上限の幅で折り返し、12 行を越える文は 12 行に収まる。同じ鍵は同じ layout を返す。
void check_fitting(IDWriteFactory &factory, IDWriteTextFormat *format)
{
    win32::HoverTipLayout held;
    IDWriteTextLayout *fitted = held.fitted(factory, format, two_rows, limit);
    const DWRITE_TEXT_METRICS wrapped = metrics_of(fitted);
    expect(fitted != nullptr && wrapped.lineCount == 2 &&
               wrapped.widthIncludingTrailingWhitespace <= static_cast<float>(limit),
           "a long hover line wraps within the limit");
    expect(held.fitted(factory, format, two_rows, limit) == fitted,
           "the same text, limit and format reuse the held layout");
    const core::LayoutRect area{0, 0, 1200, 700};
    const auto paint = held.painted(core::LayoutRect{300, 400, 302, 420}, area,
                                    core::palette_for(core::Appearance::dark), 96);
    expect(paint.has_value() && paint.value().written == fitted &&
               std::abs(paint.value().box.left - 300.0F) < 0.01F &&
               std::abs(paint.value().box.bottom - 396.0F) < 0.01F &&
               std::abs(paint.value().origin.x - paint.value().box.left - 10.0F) < 0.01F &&
               paint.value().box.right - paint.value().box.left <= 480.0F,
           "the tip is placed above the caret with the text inside the padding");
    const auto below = held.painted(core::LayoutRect{300, 10, 302, 30}, area,
                                    core::palette_for(core::Appearance::dark), 96);
    expect(below.has_value() && std::abs(below.value().box.top - 34.0F) < 0.01F,
           "the tip goes below the caret when there is no room above");
    std::string tall;
    for (std::size_t line = 0; line < core::hover_tip_max_lines; ++line)
    {
        tall += std::string(two_rows) + "\n";
    }
    IDWriteTextLayout *clipped = held.fitted(factory, format, tall, limit);
    expect(clipped != nullptr && metrics_of(clipped).lineCount == core::hover_tip_max_lines,
           "wrapped rows beyond twelve are cut to twelve rows");
    IDWriteTextLayout *unbroken = held.fitted(factory, format, std::string(120, 'W'), limit);
    expect(unbroken != nullptr && metrics_of(unbroken).lineCount > 1 &&
               metrics_of(unbroken).widthIncludingTrailingWhitespace <= static_cast<float>(limit),
           "a word longer than the limit breaks inside the word");
}

// 画素の範囲 region（札の中の左右の余白の内側）にある色 ink の画素の数。
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

// 描いた札の地・縁・2 行の文字の画素と札の外を見る。
void check_pixels(const Pixels &pixels, const D2D1_RECT_F &box, const core::Palette &palette,
                  const std::string &named)
{
    const auto left = static_cast<std::int32_t>(box.left);
    const auto middle = static_cast<std::int32_t>((box.top + box.bottom) / 2.0F);
    expect(near_color(pixel_at(pixels, left + 4, middle), palette.panel),
           ("the tip ground is the panel color" + named).c_str());
    expect(near_color(pixel_at(pixels, left, middle), palette.panel_border),
           ("the tip edge is the panel border color" + named).c_str());
    // 上の半分と下の半分のそれぞれに文字の色の画素がある（2 行）。
    const auto right = static_cast<std::int32_t>(box.right);
    const core::LayoutRect upper{left + 10, static_cast<std::int32_t>(box.top) + 6, right - 10,
                                 middle};
    const core::LayoutRect lower{left + 10, middle, right - 10,
                                 static_cast<std::int32_t>(box.bottom) - 6};
    expect(inked(pixels, upper, palette.text) > 0 && inked(pixels, lower, palette.text) > 0,
           ("both rows are written in the text color" + named).c_str());
    expect(pixel_at(pixels, left - 3, middle) == 0 &&
               pixel_at(pixels, static_cast<std::int32_t>(box.right) + 3, middle) == 0 &&
               pixel_at(pixels, left + 20, static_cast<std::int32_t>(box.bottom) + 3) == 0,
           ("nothing is drawn outside the tip" + named).c_str());
}

// 2 行の札を描き、画素を見る。
void check_paint(SoftwareTarget &target, win32::HoverTipLayout &held, const core::Palette &palette,
                 const char *theme)
{
    // 札は高さ 80 の bitmap に収まる所（キャレットの下）に置く。
    const auto paint =
        held.painted(core::LayoutRect{40, 0, 42, 6}, core::LayoutRect{0, 0, 1300, 80}, palette, 96);
    if (!paint.has_value())
    {
        unmeasured("the hover tip paint needs a DirectWrite layout");
        return;
    }
    const D2D1_RECT_F box = paint.value().box;
    const Pixels pixels =
        target.painted([&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
                       { win32::paint_tab_tip(context, brush, paint.value()); });
    check_pixels(pixels, box, palette, std::string(" (") + theme + ")");
}
} // namespace

void verify_hover_tip_paint()
{
    SoftwareTarget target;
    auto factory = directwrite_factory();
    if (!target.ready() || !factory)
    {
        unmeasured("the hover tip paint needs the software target and DirectWrite");
        return;
    }
    const TextFormat format = tip_format(*factory.Get());
    if (!format)
    {
        unmeasured("the hover tip paint needs the UI text format");
        return;
    }
    check_fitting(*factory.Get(), format.Get());
    win32::HoverTipLayout held;
    if (held.fitted(*factory.Get(), format.Get(), two_rows, limit) == nullptr)
    {
        unmeasured("the hover tip paint needs a DirectWrite layout");
        return;
    }
    check_paint(target, held, core::palette_for(core::Appearance::dark), "dark");
    check_paint(target, held, core::palette_for(core::Appearance::light), "light");
}
} // namespace nenenib::tests::ui
