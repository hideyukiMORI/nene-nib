// イージーモーションの印の描画（ADR 0163 の決定 6・8・#1782）。renderer と同じ motion_label_box と
// paint_motion_label を、本物の DirectWrite の本文の layout とメモリの bitmap で通し、地の矩形が
// 跳び先の文字の当たり矩形に揃うこと（半角・全角・2 文字の印のはみ出し・折り返した行の 2 行目・
// DPI 120 と 144）と、地を塗って字を地の中に描くこと（ダークとライトの配色の accent と
// on_accent）、印の無い行は印の前と同じ画素であることを画素で確かめる（窓・device は作らない）。
// 字体に結果が依るので環境依存（QLT-013）。
#include "BodyTextLayouts.hpp"
#include "DirectWriteFactory.hpp"
#include "MotionLabelBox.hpp"
#include "MotionLabelPaint.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "Appearance.hpp"
#include "Column.hpp"
#include "EditorSettings.hpp"
#include "MotionLabel.hpp"
#include "Palette.hpp"
#include "TextLayoutRequest.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
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

// 地の矩形が跳び先の当たり矩形に揃う（半角・全角・2 文字のはみ出し・DPI）。
void check_boxes(win32::BodyTextLayouts &layouts, std::uint32_t dpi)
{
    // 「aあ b」: a は UTF-16 の 0、あは 1（全角）。
    const auto layout = body_layout(layouts, "a\xE3\x81\x82 b", dpi, false);
    if (!layout)
    {
        unmeasured("the label boxes need a DirectWrite body layout");
        return;
    }
    const auto narrow = hit_of(layout.Get(), 0);
    const auto wide = hit_of(layout.Get(), 1);
    if (!narrow.has_value() || !wide.has_value())
    {
        unmeasured("the label boxes need the hit-test rectangles");
        return;
    }
    const auto &a = narrow.value();
    const auto &w = wide.value();
    const std::string at = " at DPI " + std::to_string(dpi);
    expect(same_box(win32::motion_label_box(a, {core::Column{1}, "a", 1}),
                    sized(a.left, a.top, a.width, a.height)),
           ("a one-letter label covers a narrow target" + at).c_str());
    expect(same_box(win32::motion_label_box(a, {core::Column{1}, ";s", 1}),
                    sized(a.left, a.top, a.width * 2.0F, a.height)),
           ("a two-letter label on a narrow target spills into the next cell" + at).c_str());
    expect(same_box(win32::motion_label_box(w, {core::Column{2}, ";s", 2}),
                    sized(w.left, w.top, w.width, w.height)),
           ("a wide target takes two cells for a two-letter label" + at).c_str());
}

// 折り返した行の 2 行目の跳び先は 2 行目の当たり矩形に揃う。
void check_wrapped(win32::BodyTextLayouts &layouts)
{
    const auto layout = body_layout(layouts, "aaaa bbbb cccc dddd", 96, true);
    if (!layout)
    {
        unmeasured("the wrapped label needs a DirectWrite body layout");
        return;
    }
    const auto last = hit_of(layout.Get(), 15);
    if (!last.has_value())
    {
        unmeasured("the wrapped label needs the hit-test rectangle");
        return;
    }
    const auto &hit = last.value();
    expect(hit.top > 0.0F && same_box(win32::motion_label_box(hit, {core::Column{16}, "a", 1}),
                                      sized(hit.left, hit.top, hit.width, hit.height)),
           "a target on a wrapped row is boxed on that row");
}

// 地の中で地の色と違う画素の数（印の字が描いた画素）。
[[nodiscard]] std::size_t inked_in(const Pixels &pixels, const D2D1_RECT_F &box,
                                   std::uint32_t ground)
{
    std::size_t inked = 0;
    for (auto x = static_cast<std::int32_t>(box.left); x < static_cast<std::int32_t>(box.right);
         ++x)
    {
        for (auto y = static_cast<std::int32_t>(box.top); y < static_cast<std::int32_t>(box.bottom);
             ++y)
        {
            inked += pixel_at(pixels, x, y) != ground ? 1U : 0U;
        }
    }
    return inked;
}

// 地を accent で塗り、字を on_accent で地の中に描く。地の外は描かない。
void check_paint(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                 const core::Palette &palette, const char *theme)
{
    const auto layout = body_layout(layouts, "ab cd", 96, false);
    auto factory = directwrite_factory();
    if (!layout || !factory)
    {
        unmeasured("the label paint needs a DirectWrite layout and factory");
        return;
    }
    const auto hit = hit_of(layout.Get(), 3);
    if (!hit.has_value())
    {
        unmeasured("the label paint needs the hit-test rectangle");
        return;
    }
    const core::MotionLabel label{core::Column{4}, "s", 1};
    const D2D1_RECT_F box = win32::motion_label_box(hit.value(), label);
    const std::wstring wide = L"s";
    Microsoft::WRL::ComPtr<IDWriteTextLayout> written;
    if (FAILED(factory->CreateTextLayout(wide.c_str(), 1, layout.Get(), box.right - box.left,
                                         box.bottom - box.top, &written)))
    {
        unmeasured("the label paint needs the label's own text layout");
        return;
    }
    const auto ground = color_of(palette.accent);
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::paint_motion_label(
                context, brush,
                win32::MotionLabelPaint{box, written.Get(), ground, color_of(palette.on_accent)});
        });
    const auto expected_ground = pixel_at(pixels, static_cast<std::int32_t>(box.left) + 1,
                                          static_cast<std::int32_t>(box.top) + 1);
    const std::size_t inked = inked_in(pixels, box, expected_ground);
    const std::string named = std::string(" (") + theme + ")";
    expect(expected_ground != 0, ("the label ground is painted" + named).c_str());
    expect(inked > 0, ("the label letter is drawn inside the ground" + named).c_str());
    expect(pixel_at(pixels, static_cast<std::int32_t>(std::ceil(box.right)) + 3,
                    static_cast<std::int32_t>(box.top) + 1) == 0 &&
               pixel_at(pixels, static_cast<std::int32_t>(std::floor(box.left)) - 3,
                        static_cast<std::int32_t>(box.top) + 1) == 0,
           ("nothing is drawn outside the label ground" + named).c_str());
}

// 地の外の画素は印の前と同じ（印は地の外の本文を描き直さない）。
[[nodiscard]] std::size_t changed_outside(const Pixels &before, const Pixels &after,
                                          const D2D1_RECT_F &box)
{
    std::size_t changed = 0;
    for (std::int32_t y = 0; y < static_cast<std::int32_t>(SoftwareTarget::height); ++y)
    {
        for (std::int32_t x = 0; x < static_cast<std::int32_t>(SoftwareTarget::width); ++x)
        {
            const bool inside = x >= static_cast<std::int32_t>(std::floor(box.left)) - 1 &&
                                x <= static_cast<std::int32_t>(std::ceil(box.right)) + 1 &&
                                y >= static_cast<std::int32_t>(std::floor(box.top)) - 1 &&
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
    const D2D1_RECT_F box = win32::motion_label_box(hit.value(), {core::Column{4}, "a", 1});
    const auto text = [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
    {
        context.DrawTextLayout(D2D1::Point2F(0, 0), layout.Get(), brush,
                               D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    const Pixels before = target.painted(text);
    const Pixels after = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            text(context, brush);
            win32::paint_motion_label(context, brush,
                                      win32::MotionLabelPaint{box, nullptr, D2D1::ColorF(1, 0, 0),
                                                              D2D1::ColorF(0, 0, 1)});
        });
    expect(!before.empty() && changed_outside(before, after, box) == 0,
           "the pixels outside the label ground are the same as before the label");
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
    check_boxes(layouts, 96);
    check_boxes(layouts, 120);
    check_boxes(layouts, 144);
    check_wrapped(layouts);
    check_paint(target, layouts, core::palette_for(core::Appearance::dark), "dark");
    check_paint(target, layouts, core::palette_for(core::Appearance::light), "light");
    check_unlabelled(target, layouts);
}
} // namespace nenenib::tests::ui
