// イージーモーションの印の描画（ADR 0163 の決定 6・8・#1782・追補 #1800 の案 D'）。renderer と同じ
// motion_label_badge・paint_motion_label を、本物の DirectWrite の本文の layout とメモリの bitmap
// で通し、札が跳び先の自分の行の中の上寄せに整数の物理画素で置かれること（全角の隣り合う 2 文字の
// 札・空の行・折り返した行の 2 行目・DPI 144・行の 6 割の高さ）、札の中央が text・まわりの 1 画素
// が background・上の行の画素が変わらない・跳び先の字の札より下が印の前と同じこと（ダークとライト
// の配色・角の外のアンチエイリアスの画素は比べない）、札の外は印の前と同じ画素であること、印が
// 出ている間のブロックのキャレットは枠だけで中を塗らないことを画素で確かめる（窓・device は
// 作らない）。字体に結果が依るので環境依存（QLT-013）。
#include "BlockCaretOutline.hpp"
#include "BodyTextLayouts.hpp"
#include "DirectWriteFactory.hpp"
#include "MotionLabelBox.hpp"
#include "MotionLabelPaint.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "Appearance.hpp"
#include "Column.hpp"
#include "EditorSettings.hpp"
#include "FontSize.hpp"
#include "LineView.hpp"
#include "MotionLabel.hpp"
#include "Palette.hpp"
#include "TextLayoutRequest.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

constexpr std::int32_t line_height = 20;
constexpr std::size_t channels = 4;
constexpr float near_pixel = 0.01F;

[[nodiscard]] D2D1_COLOR_F color_of(core::RgbColor color) noexcept
{
    constexpr float full = 255.0F;
    return D2D1::ColorF(static_cast<float>(color.red) / full,
                        static_cast<float>(color.green) / full,
                        static_cast<float>(color.blue) / full);
}

// 2 つの画素の色が近い（各色の差が tolerance 以内）。
[[nodiscard]] bool near_color(std::uint32_t pixel, std::uint32_t expected) noexcept
{
    constexpr std::int32_t tolerance = 2;
    for (std::uint32_t shift = 0; shift <= 16U; shift += 8U)
    {
        const auto a = static_cast<std::int32_t>((pixel >> shift) & 0xFFU);
        const auto b = static_cast<std::int32_t>((expected >> shift) & 0xFFU);
        if (std::abs(a - b) > tolerance)
        {
            return false;
        }
    }
    return true;
}

// 色の BGRA の並び（pixel_at と同じ形）。
[[nodiscard]] std::uint32_t bgr_of(core::RgbColor color) noexcept
{
    return static_cast<std::uint32_t>(color.blue) |
           (static_cast<std::uint32_t>(color.green) << 8U) |
           (static_cast<std::uint32_t>(color.red) << 16U);
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

[[nodiscard]] std::optional<DWRITE_HIT_TEST_METRICS> hit_of(IDWriteTextLayout *layout,
                                                            UINT32 position)
{
    DWRITE_HIT_TEST_METRICS hit{};
    UINT32 count = 0;
    if (FAILED(layout->HitTestTextRange(position, 1, 0, 0, &hit, 1, &count)) || count != 1)
    {
        return std::nullopt;
    }
    return hit;
}

[[nodiscard]] bool same_box(const D2D1_RECT_F &box, const D2D1_RECT_F &expected) noexcept
{
    return std::abs(box.left - expected.left) < near_pixel &&
           std::abs(box.top - expected.top) < near_pixel &&
           std::abs(box.right - expected.right) < near_pixel &&
           std::abs(box.bottom - expected.bottom) < near_pixel;
}

[[nodiscard]] bool close_to(float value, float expected) noexcept
{
    return std::abs(value - expected) < near_pixel;
}

// 左上と幅・高さの矩形。
[[nodiscard]] D2D1_RECT_F sized(float left, float top, float width, float height) noexcept
{
    return D2D1::RectF(left, top, left + width, top + height);
}

using BodyLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

// 本文の layout（作れなければ空）。
[[nodiscard]] BodyLayout body_layout(win32::BodyTextLayouts &layouts, const char *text,
                                     std::uint32_t dpi, bool wrapped)
{
    auto settings = core::default_editor_settings();
    settings.wrapping = wrapped ? core::LineWrapping::wrap : core::LineWrapping::no_wrap;
    const std::int32_t width = wrapped ? 60 : 1200;
    const application::TextLayoutRequest request{text, settings,
                                                 core::TextViewGeometry{width, line_height, dpi}};
    auto made = layouts.get(request);
    if (!made.has_value())
    {
        return nullptr;
    }
    return std::move(made).value();
}

// 枠の内側（枠から 2 画素離した所）の画素のうち、条件に合う数。
template <typename Pick>
[[nodiscard]] std::size_t inside_count(const Pixels &pixels, const D2D1_RECT_F &box, Pick pick)
{
    std::size_t count = 0;
    for (auto x = static_cast<std::int32_t>(std::ceil(box.left)) + 2;
         x < static_cast<std::int32_t>(std::floor(box.right)) - 2; ++x)
    {
        for (auto y = static_cast<std::int32_t>(std::ceil(box.top)) + 2;
             y < static_cast<std::int32_t>(std::floor(box.bottom)) - 2; ++y)
        {
            count += pick(pixel_at(pixels, x, y)) ? 1U : 0U;
        }
    }
    return count;
}

// 印の字の layout（製品の印の書式と同じ太字・本文の 6 割・札の大きさに広げて中央に置く）。
// 作れなければ空。
[[nodiscard]] BodyLayout label_layout(const wchar_t *text)
{
    auto factory = directwrite_factory();
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    BodyLayout made;
    const float size = win32::motion_label_text_dips(
        core::font_size_dips(core::default_editor_settings().font_size));
    if (!factory ||
        FAILED(factory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_BOLD,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
                                         L"", format.GetAddressOf())) ||
        FAILED(factory->CreateTextLayout(text, 1, format.Get(), 400.0F, 100.0F,
                                         made.GetAddressOf())) ||
        FAILED(made->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER)) ||
        FAILED(made->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)))
    {
        return nullptr;
    }
    return made;
}

// 字の大きさの測り（幅・行の高さ）。測れなければ 0。
[[nodiscard]] D2D1_SIZE_F measured(IDWriteTextLayout *layout)
{
    DWRITE_TEXT_METRICS metrics{};
    if (layout == nullptr || FAILED(layout->GetMetrics(&metrics)))
    {
        return D2D1::SizeF(0.0F, 0.0F);
    }
    return D2D1::SizeF(metrics.widthIncludingTrailingWhitespace, metrics.height);
}

// 当たり矩形を縦に dy ずらす（2 行目の行の layout を y = dy に置く）。
[[nodiscard]] DWRITE_HIT_TEST_METRICS lowered(DWRITE_HIT_TEST_METRICS hit, float dy) noexcept
{
    hit.top += dy;
    return hit;
}

// 札の寸法（案 D'・#1800）: 札は自分の行の中の上寄せ（上端は行の上端 + 1 画素・左端は升の左端）、
// 高さは字の行の高さで行の 6 割まで、幅は字 + 左右 2 DIP までの余白で升 × 印の字の数を超えない。
// 半径は物理画素で 2〜3。整数の物理画素。
void check_badges()
{
    DWRITE_HIT_TEST_METRICS cell{};
    cell.left = 40.3F;
    cell.top = 20.0F;
    cell.width = 9.0F;
    cell.height = 20.0F;
    const core::MotionLabel one{core::Column{1}, "a", 1};
    const D2D1_SIZE_F written = D2D1::SizeF(7.2F, 13.4F);
    const D2D1_ROUNDED_RECT at_96 = win32::motion_label_badge(cell, one, written, 96);
    expect(same_box(at_96.rect, D2D1::RectF(40.0F, 21.0F, 50.0F, 33.0F)) &&
               close_to(at_96.radiusX, 2.0F) && close_to(at_96.radiusY, 2.0F),
           "the badge sits in its own row, a pixel below the top, at the cell's left edge");
    expect(same_box(win32::motion_label_badge(cell, one, D2D1::SizeF(4.2F, 13.4F), 96).rect,
                    D2D1::RectF(40.0F, 21.0F, 49.0F, 33.0F)),
           "a narrow letter keeps its side padding within one cell");
    const D2D1_ROUNDED_RECT at_144 = win32::motion_label_badge(cell, one, written, 144);
    expect(at_144.rect.right - at_144.rect.left <= 10.0F && close_to(at_144.radiusX, 3.0F),
           "the badge keeps a one-pixel side padding past a tight cell and the radius follows "
           "the DPI at 144");
    expect(close_to(win32::motion_label_badge(cell, one, written, 288).radiusX, 3.0F),
           "the badge radius stays within three pixels");
    const core::MotionLabel two{core::Column{1}, ";s", 1};
    expect(win32::motion_label_badge(cell, two, D2D1::SizeF(14.0F, 13.4F), 96).rect.right > 49.0F,
           "a two-letter badge on a narrow target spills into the next cell");
    DWRITE_HIT_TEST_METRICS wide = cell;
    wide.width = 18.0F;
    DWRITE_HIT_TEST_METRICS next_wide = wide;
    next_wide.left = 58.3F;
    const core::MotionLabel wide_two{core::Column{1}, ";s", 2};
    const auto left_badge =
        win32::motion_label_badge(wide, wide_two, D2D1::SizeF(12.0F, 13.4F), 96);
    const auto right_badge =
        win32::motion_label_badge(next_wide, wide_two, D2D1::SizeF(12.0F, 13.4F), 96);
    expect(left_badge.rect.right - left_badge.rect.left <= 18.0F &&
               left_badge.rect.right + 1.0F <= right_badge.rect.left - 1.0F,
           "two-letter badges on adjacent wide targets do not overlap, edges included");
    expect(win32::motion_label_badge(wide, {core::Column{1}, "a", 2}, written, 96).rect.right <=
               50.0F,
           "a one-letter badge on a wide target stays in the left cell and one pixel past it");
    expect(win32::motion_label_badge(cell, one, D2D1::SizeF(7.2F, 30.0F), 96).rect.bottom <= 33.0F,
           "the badge is no taller than six tenths of the row");
    expect(at_96.rect.top - 1.0F >= cell.top, "nothing of the badge is above its row");
    expect(close_to(win32::motion_label_text_dips(18.0F), 10.8F) &&
               close_to(win32::motion_label_text_dips(12.0F), 10.0F),
           "the badge letter is six tenths of the body and never below 10 DIP");
}

// 折り返した行の 2 行目の跳び先の札は 2 行目の中に置く。
void check_wrapped(win32::BodyTextLayouts &layouts)
{
    const auto layout = body_layout(layouts, "aaaa bbbb cccc dddd", 96, true);
    const auto last = layout ? hit_of(layout.Get(), 15) : std::nullopt;
    if (!last.has_value())
    {
        unmeasured("the wrapped label needs a DirectWrite layout");
        return;
    }
    const auto &hit = last.value();
    const auto badge =
        win32::motion_label_badge(hit, {core::Column{16}, "a", 1}, D2D1::SizeF(5.0F, 13.0F), 96);
    expect(hit.top > 0.0F && close_to(badge.rect.top, std::floor(hit.top + 0.5F) + 1.0F) &&
               close_to(badge.rect.left, std::floor(hit.left + 0.5F)),
           "a target on a wrapped row is labelled inside that row");
}

// 札の字の layout を作り、札を求めて大きさを広げる（製品の draw_label と同じ順）。
[[nodiscard]] std::optional<D2D1_ROUNDED_RECT> placed_badge(IDWriteTextLayout *written,
                                                            const DWRITE_HIT_TEST_METRICS &target,
                                                            const core::MotionLabel &label)
{
    const auto badge = win32::motion_label_badge(target, label, measured(written), 96);
    if (FAILED(written->SetMaxWidth(badge.rect.right - badge.rect.left)) ||
        FAILED(written->SetMaxHeight(badge.rect.bottom - badge.rect.top)))
    {
        return std::nullopt;
    }
    return badge;
}

// 札の地の中ほど（字の外の左の余白・角の半径より下）の画素。
[[nodiscard]] std::uint32_t ground_at(const Pixels &pixels, const D2D1_ROUNDED_RECT &badge)
{
    return pixel_at(pixels, static_cast<std::int32_t>(badge.rect.left) + 1,
                    static_cast<std::int32_t>((badge.rect.top + badge.rect.bottom) / 2.0F));
}

// 札を描いた画素の組（印の前・字の無い札・字のある札）と札と跳び先の升。
using PaintedLabel = std::tuple<const Pixels &, const Pixels &, const Pixels &,
                                const D2D1_ROUNDED_RECT &, const DWRITE_HIT_TEST_METRICS &>;

// 跳び先の升のうち札（と縁）より下の画素が印の前と同じで、そこに字の画素がある。
[[nodiscard]] bool target_kept(const Pixels &before, const Pixels &after,
                               const DWRITE_HIT_TEST_METRICS &cell, const D2D1_RECT_F &badge)
{
    std::size_t differing = 0;
    std::size_t inked = 0;
    for (auto x = static_cast<std::int32_t>(std::ceil(cell.left));
         x < static_cast<std::int32_t>(cell.left + cell.width); ++x)
    {
        for (auto y = static_cast<std::int32_t>(badge.bottom) + 2;
             y < static_cast<std::int32_t>(cell.top + cell.height); ++y)
        {
            differing += pixel_at(before, x, y) == pixel_at(after, x, y) ? 0U : 1U;
            inked += pixel_at(before, x, y) != 0 ? 1U : 0U;
        }
    }
    return differing == 0 && inked > 0;
}

void expect_painted(const PaintedLabel &painted, const core::Palette &palette,
                    const std::string &named)
{
    const auto &[before, ground, after, badge, cell] = painted;
    const auto row = static_cast<float>(line_height);
    const auto middle = static_cast<std::int32_t>((badge.rect.top + badge.rect.bottom) / 2.0F);
    const auto centre = static_cast<std::int32_t>((badge.rect.left + badge.rect.right) / 2.0F);
    expect(near_color(pixel_at(ground, centre, middle), bgr_of(palette.text)),
           ("the badge's centre is the text colour" + named).c_str());
    expect(near_color(pixel_at(after, static_cast<std::int32_t>(badge.rect.left) - 1, middle),
                      bgr_of(palette.background)),
           ("a one-pixel background edge rings the badge" + named).c_str());
    std::size_t above = 0;
    for (auto x = static_cast<std::int32_t>(badge.rect.left) - 2;
         x <= static_cast<std::int32_t>(badge.rect.right) + 1; ++x)
    {
        above += pixel_at(before, x, static_cast<std::int32_t>(row) - 1) ==
                         pixel_at(after, x, static_cast<std::int32_t>(row) - 1)
                     ? 0U
                     : 1U;
    }
    expect(above == 0 && near_color(ground_at(ground, badge), bgr_of(palette.text)),
           ("nothing of the badge is drawn above its row" + named).c_str());
    expect(inside_count(after, badge.rect, [&](std::uint32_t pixel)
                        { return !near_color(pixel, bgr_of(palette.text)); }) > 0,
           ("the label letter is drawn on the badge" + named).c_str());
    expect(target_kept(before, after, cell, badge.rect),
           ("the target letter below the badge is the same as before the label" + named).c_str());
}

// 2 行を描いてから札を描く（印の 1 パスと同じ順）。2 行目の跳び先 g に札を置き、札の中央が text・
// 札のまわりの 1 画素が background・上の行の画素は 1 つも変わらない・跳び先の升の札より下は
// 印の前と同じ（跳び先の字の下半分が見える・g
// の下の払いで字の画素を確かめる）ことを確かめる。角の外のアンチエイリアスの画素は比べない。
void check_paint(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                 const core::Palette &palette, const char *theme)
{
    const auto above = body_layout(layouts, "xy zw", 96, false);
    const auto below = body_layout(layouts, "ab gd", 96, false);
    const auto hit = below ? hit_of(below.Get(), 3) : std::nullopt;
    const auto written = label_layout(L"s");
    if (!above || !hit.has_value() || !written)
    {
        unmeasured("the label paint needs DirectWrite layouts");
        return;
    }
    const auto row = static_cast<float>(line_height);
    const DWRITE_HIT_TEST_METRICS cell = lowered(hit.value(), row);
    const auto placed = placed_badge(written.Get(), cell, {core::Column{4}, "s", 1});
    if (!placed.has_value())
    {
        unmeasured("the label paint needs the badge layout");
        return;
    }
    const auto &badge = placed.value();
    const auto lines = [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
    {
        context.DrawTextLayout(D2D1::Point2F(0, 0), above.Get(), brush,
                               D2D1_DRAW_TEXT_OPTIONS_CLIP);
        context.DrawTextLayout(D2D1::Point2F(0, row), below.Get(), brush,
                               D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    const auto labelled = [&](IDWriteTextLayout *letter)
    {
        return target.painted(
            [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
            {
                lines(context, brush);
                win32::paint_motion_label(context, brush,
                                          win32::MotionLabelPaint{badge, letter,
                                                                  color_of(palette.text),
                                                                  color_of(palette.background)});
            });
    };
    const Pixels before = target.painted(lines);
    const Pixels ground = labelled(nullptr);
    const Pixels after = labelled(written.Get());
    expect_painted(PaintedLabel{before, ground, after, badge, cell}, palette,
                   std::string(" (") + theme + ")");
}

// 空の行（字の無い跳び先）の札も字のある跳び先と同じく行の中の上寄せ。跳び先は左に 1 升あけた
// 位置に置く。上下に並んだ 2 つの行の札は重ならない。
void check_empty_line(SoftwareTarget &target)
{
    const auto written = label_layout(L"a");
    if (!written)
    {
        unmeasured("the empty-line label needs a DirectWrite layout");
        return;
    }
    DWRITE_HIT_TEST_METRICS hit{};
    hit.left = 10.0F;
    hit.top = static_cast<float>(line_height);
    hit.height = static_cast<float>(line_height);
    const core::MotionLabel label{core::Column{1}, "a", 1};
    const auto placed = placed_badge(written.Get(), hit, label);
    if (!placed.has_value())
    {
        unmeasured("the empty-line label needs the badge layout");
        return;
    }
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::paint_motion_label(context, brush,
                                      win32::MotionLabelPaint{placed.value(), nullptr,
                                                              D2D1::ColorF(1, 0, 0),
                                                              D2D1::ColorF(0, 0, 1)});
        });
    const auto &shape = placed.value().rect;
    expect(ground_at(pixels, placed.value()) == 0xFF0000U,
           "an empty line's badge ground is painted");
    expect(close_to(shape.left, 10.0F) && close_to(shape.top, hit.top + 1.0F),
           "an empty line's badge sits inside its row at the cell's left edge");
    DWRITE_HIT_TEST_METRICS next = hit;
    next.top += static_cast<float>(line_height);
    const auto upper = win32::motion_label_badge(hit, label, measured(written.Get()), 96);
    const auto lower = win32::motion_label_badge(next, label, measured(written.Get()), 96);
    expect(upper.rect.bottom + 1.0F <= lower.rect.top - 1.0F,
           "the badges of two stacked rows do not overlap, edges included");
}

// 全角（かな）の字を含む行の札（#1800 の実機: 2 行目の 1 文字の札が半分に欠けて見えた）。かなは
// 代わりの字体で幅が升 2 つより狭く、1 文字の札の余白が 0 に詰まって字の画が札の縁に付き、札が
// 字の上の帽子だけに見えた。半角の跳び先の札と同じ高さで、左右の端の列と上下の端の行が地の色で
// 残る（字の画が縁に付かない）ことを画素で確かめる。
void check_wide_row(SoftwareTarget &target, win32::BodyTextLayouts &layouts)
{
    const auto layout = body_layout(layouts, "ab \xE3\x81\xAE\xE6\x96\x87", 96, false);
    const auto narrow = layout ? hit_of(layout.Get(), 0) : std::nullopt;
    const auto kana = layout ? hit_of(layout.Get(), 3) : std::nullopt;
    const auto written = label_layout(L"s");
    if (!narrow.has_value() || !kana.has_value() || !written)
    {
        unmeasured("the full-width row label needs DirectWrite layouts");
        return;
    }
    const auto ascii = placed_badge(written.Get(), narrow.value(), {core::Column{1}, "s", 1});
    const auto placed = placed_badge(written.Get(), kana.value(), {core::Column{4}, "s", 2});
    if (!ascii.has_value() || !placed.has_value())
    {
        unmeasured("the full-width row label needs the badge layout");
        return;
    }
    const auto &shape = placed.value().rect;
    const auto &plain = ascii.value().rect;
    expect(close_to(shape.bottom - shape.top, plain.bottom - plain.top) &&
               close_to(shape.top, plain.top),
           "a badge on a kana target is as tall as one on a half-width target in the same row");
    const auto palette = core::palette_for(core::Appearance::dark);
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            context.DrawTextLayout(D2D1::Point2F(0, 0), layout.Get(), brush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
            win32::paint_motion_label(context, brush,
                                      win32::MotionLabelPaint{placed.value(), written.Get(),
                                                              color_of(palette.text),
                                                              color_of(palette.background)});
        });
    const auto ground = bgr_of(palette.text);
    const auto left = static_cast<std::int32_t>(shape.left);
    const auto right = static_cast<std::int32_t>(shape.right) - 1;
    const auto top = static_cast<std::int32_t>(shape.top);
    const auto bottom = static_cast<std::int32_t>(shape.bottom) - 1;
    const auto middle = (top + bottom) / 2;
    const auto centre = (left + right) / 2;
    expect(near_color(pixel_at(pixels, left, middle), ground) &&
               near_color(pixel_at(pixels, right, middle), ground),
           "a badge on a kana target keeps ground on both sides of its letter");
    expect(near_color(pixel_at(pixels, centre, top), ground) &&
               near_color(pixel_at(pixels, centre, bottom), ground),
           "a badge on a kana target keeps ground above and below its letter");
}

// 札の外（縁の 1 画素と余裕の 1 画素を除く）の画素は印の前と同じ。
[[nodiscard]] std::size_t changed_outside(const Pixels &before, const Pixels &after,
                                          const D2D1_RECT_F &box)
{
    std::size_t changed = 0;
    for (std::int32_t y = 0; y < static_cast<std::int32_t>(SoftwareTarget::height); ++y)
    {
        for (std::int32_t x = 0; x < static_cast<std::int32_t>(SoftwareTarget::width); ++x)
        {
            const bool inside = x >= static_cast<std::int32_t>(std::floor(box.left)) - 2 &&
                                x <= static_cast<std::int32_t>(std::ceil(box.right)) + 1 &&
                                y >= static_cast<std::int32_t>(std::floor(box.top)) - 2 &&
                                y <= static_cast<std::int32_t>(std::ceil(box.bottom)) + 1;
            changed += !inside && pixel_at(before, x, y) != pixel_at(after, x, y) ? 1U : 0U;
        }
    }
    return changed;
}

void check_unlabelled(SoftwareTarget &target, win32::BodyTextLayouts &layouts)
{
    const auto layout = body_layout(layouts, "ab cd", 96, false);
    const auto hit = layout ? hit_of(layout.Get(), 3) : std::nullopt;
    if (!hit.has_value())
    {
        unmeasured("the unlabelled pixels need a DirectWrite layout");
        return;
    }
    const auto row = static_cast<float>(line_height);
    const auto badge = win32::motion_label_badge(
        lowered(hit.value(), row), {core::Column{4}, "a", 1}, D2D1::SizeF(7.0F, 13.0F), 96);
    const auto text = [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
    {
        context.DrawTextLayout(D2D1::Point2F(0, row), layout.Get(), brush,
                               D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    const Pixels before = target.painted(text);
    const Pixels after = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            text(context, brush);
            win32::paint_motion_label(context, brush,
                                      win32::MotionLabelPaint{badge, nullptr, D2D1::ColorF(1, 0, 0),
                                                              D2D1::ColorF(0, 0, 1)});
        });
    expect(!before.empty() && changed_outside(before, after, badge.rect) == 0,
           "the pixels outside the badge are the same as before the label");
}

// 印が出ている間のブロックのキャレット（#1800）: 升を accent の 1 画素の枠で囲み、中は塗らない
// （下の字は本文の色のまま）。印が出ているかは見えている行の labels の 1 つで決まる。
void check_labelled_caret(SoftwareTarget &target, win32::BodyTextLayouts &layouts)
{
    std::vector<application::LineView> lines(2);
    expect(!win32::motion_labels_shown(lines), "no labels on the lines: the caret stays a block");
    lines.back().labels.push_back(core::MotionLabel{core::Column{4}, "a", 1});
    expect(win32::motion_labels_shown(lines), "a label on any line: the caret is outlined");
    const auto layout = body_layout(layouts, "ab cd", 96, false);
    const auto hit = layout ? hit_of(layout.Get(), 0) : std::nullopt;
    if (!hit.has_value())
    {
        unmeasured("the outlined caret needs a DirectWrite layout");
        return;
    }
    const auto &a = hit.value();
    const D2D1_RECT_F block = sized(a.left, a.top, a.width, a.height);
    const auto accent = core::palette_for(core::Appearance::dark).accent;
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            context.DrawTextLayout(D2D1::Point2F(0, 0), layout.Get(), brush,
                                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
            brush->SetColor(color_of(accent));
            win32::outline_caret_block(context, brush, block, 1.0F);
        });
    const auto middle = static_cast<std::int32_t>((block.top + block.bottom) / 2.0F);
    expect(near_color(pixel_at(pixels, static_cast<std::int32_t>(std::round(block.left)), middle),
                      bgr_of(accent)),
           "the outlined caret's edge is accent");
    expect(inside_count(pixels, block, [&](std::uint32_t pixel)
                        { return near_color(pixel, bgr_of(accent)); }) == 0,
           "the outlined caret is not filled with accent");
    expect(inside_count(pixels, block, [](std::uint32_t pixel) { return pixel != 0; }) > 0,
           "the letter under the outlined caret keeps the body colour");
}
} // namespace

void verify_motion_label_paint()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the label paint needs the software target");
        return;
    }
    win32::BodyTextLayouts layouts;
    check_badges();
    check_wrapped(layouts);
    check_paint(target, layouts, core::palette_for(core::Appearance::dark), "dark");
    check_paint(target, layouts, core::palette_for(core::Appearance::light), "light");
    check_unlabelled(target, layouts);
    check_empty_line(target);
    check_wide_row(target, layouts);
    check_labelled_caret(target, layouts);
}
} // namespace nenenib::tests::ui
