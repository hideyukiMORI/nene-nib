// matchparen の相手の括弧の枠（ADR 0164 の決定 4・Issue #999）。renderer は current_match と同じ
// BodyTextPainter::outline_runs で、範囲の当たり矩形を内側 1 DIP の枠で囲む。本物の DirectWrite の
// layout とメモリの bitmap で描き、枠の縁だけが光って括弧の中と外は塗らないことを画素で確かめる
// （窓・device は作らない）。字体に結果が依るので環境依存（QLT-013）。
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "EditorSettings.hpp"
#include "RenderFailure.hpp"
#include "TextLayoutRequest.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

constexpr std::int32_t line_height = 20;
constexpr std::int32_t layout_width = 1200;
constexpr nenenib::core::LayoutRect area{0, 0, layout_width, 40};
// 「f(a)」の `)`（UTF-16 の 3 単位目）。
constexpr DWRITE_TEXT_RANGE partner{3, 1};
constexpr std::size_t channels = 4;

[[nodiscard]] bool lit(const Pixels &pixels, std::int32_t x, std::int32_t y)
{
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(SoftwareTarget::width) ||
        y >= static_cast<std::int32_t>(SoftwareTarget::height))
    {
        return false;
    }
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
        channels;
    return pixels.at(at) != 0 || pixels.at(at + 1) != 0 || pixels.at(at + 2) != 0;
}

// x の前後 1 画素のどれかが光るか（1 DIP の線は小数の位置で 2 画素にまたがる）。
[[nodiscard]] bool lit_near(const Pixels &pixels, float x, std::int32_t y)
{
    const auto column = static_cast<std::int32_t>(std::floor(x));
    return lit(pixels, column - 1, y) || lit(pixels, column, y) || lit(pixels, column + 1, y);
}

void check_outline(SoftwareTarget &target, win32::BodyTextLayouts &layouts,
                   IDWriteTextLayout *layout)
{
    DWRITE_HIT_TEST_METRICS hit{};
    UINT32 count = 0;
    if (FAILED(layout->HitTestTextRange(partner.startPosition, partner.length, 0, 0, &hit, 1,
                                        &count)) ||
        count != 1)
    {
        unmeasured("the bracket outline needs the hit-test rectangle of the partner");
        return;
    }
    std::optional<win32::RenderFailure> failure;
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::BodyTextPainter<ID2D1RenderTarget> painter(context, brush, layouts, failure);
            painter.outline_runs(layout, area, partner, 1.0F);
        });
    const auto middle = static_cast<std::int32_t>(hit.top + (hit.height / 2.0F));
    const float right = hit.left + hit.width;
    expect(!pixels.empty() && !failure.has_value(), "the outline is drawn without a failure");
    expect(lit_near(pixels, hit.left + 0.5F, middle) && lit_near(pixels, right - 0.5F, middle),
           "the partner's outline lights both vertical edges of its hit-test rectangle");
    expect(lit(pixels, static_cast<std::int32_t>(hit.left + (hit.width / 2.0F)),
               static_cast<std::int32_t>(hit.top)),
           "the outline lights the top edge");
    expect(!lit(pixels, static_cast<std::int32_t>(hit.left + (hit.width / 2.0F)), middle),
           "the inside of the outline is not filled");
    expect(!lit(pixels, static_cast<std::int32_t>(std::floor(hit.left)) - 4, middle) &&
               !lit(pixels, static_cast<std::int32_t>(std::ceil(right)) + 4, middle),
           "nothing is drawn outside the partner's rectangle");
}
} // namespace

void verify_matchparen_outline()
{
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the bracket outline needs the software target");
        return;
    }
    win32::BodyTextLayouts layouts;
    auto settings = nenenib::core::default_editor_settings();
    settings.wrapping = nenenib::core::LineWrapping::no_wrap;
    const nenenib::application::TextLayoutRequest request{
        "f(a)", settings, nenenib::core::TextViewGeometry{layout_width, line_height, 96}};
    const auto layout = layouts.get(request);
    if (!layout.has_value())
    {
        unmeasured("the bracket outline needs a DirectWrite body layout");
        return;
    }
    check_outline(target, layouts, layout.value().Get());
}
} // namespace nenenib::tests::ui
