// `:s///c` の問いの描画（ADR 0168 の決定 6・Issue
// #1652）。入力行の前に描くプロンプトは問いでは空で、 描く文は frame の入力行の文（Vim の `replace
// with … (y/n/a/q/l)?`）だけになる。問うている当たりの 枠は今の一致（ADR 0037 の accent
// の枠）と同じ BodyTextPainter::outline_runs で、本物の DirectWrite の layout とメモリの bitmap
// に描き、当たりの縁が光り、ほかの当たりは塗らないことを画素で確かめる （窓・device
// は作らない）。字体に結果が依るので環境依存（QLT-013）。
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "InputLinePromptText.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include "DisplayLanguage.hpp"
#include "EditorSettings.hpp"
#include "InputLinePrompt.hpp"
#include "RenderFailure.hpp"
#include "TextLayoutRequest.hpp"

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
// 「a a a」の 2 つ目の `a`（問うている当たり・UTF-16 の 3 単位目）。
constexpr DWRITE_TEXT_RANGE asked{2, 1};
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

[[nodiscard]] bool lit_near(const Pixels &pixels, float x, std::int32_t y)
{
    const auto column = static_cast<std::int32_t>(std::floor(x));
    return lit(pixels, column - 1, y) || lit(pixels, column, y) || lit(pixels, column + 1, y);
}

void check_frame(SoftwareTarget &target, win32::BodyTextLayouts &layouts, IDWriteTextLayout *layout)
{
    DWRITE_HIT_TEST_METRICS hit{};
    DWRITE_HIT_TEST_METRICS first{};
    UINT32 count = 0;
    UINT32 counted = 0;
    if (FAILED(
            layout->HitTestTextRange(asked.startPosition, asked.length, 0, 0, &hit, 1, &count)) ||
        FAILED(layout->HitTestTextRange(0, 1, 0, 0, &first, 1, &counted)) || count != 1 ||
        counted != 1)
    {
        unmeasured("the asked match frame needs the hit-test rectangles");
        return;
    }
    std::optional<win32::RenderFailure> failure;
    const Pixels pixels = target.painted(
        [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
        {
            win32::BodyTextPainter<ID2D1RenderTarget> painter(context, brush, layouts, failure);
            painter.outline_runs(layout, area, asked, 1.0F);
        });
    const auto middle = static_cast<std::int32_t>(hit.top + (hit.height / 2.0F));
    expect(!pixels.empty() && !failure.has_value(), "the asked match frame is drawn");
    expect(lit_near(pixels, hit.left + 0.5F, middle) &&
               lit_near(pixels, hit.left + hit.width - 0.5F, middle),
           "the frame lights both edges of the asked match");
    expect(!lit(pixels, static_cast<std::int32_t>(first.left + (first.width / 2.0F)),
                static_cast<std::int32_t>(first.top)),
           "the other matches are not framed");
}
} // namespace

void verify_substitute_confirm_paint()
{
    namespace core = nenenib::core;
    expect(win32::prompt_text(core::InputLinePrompt::substitute_confirm, {},
                              core::DisplayLanguage::japanese)
                   .empty() &&
               win32::prompt_text(core::InputLinePrompt::ex, {}, core::DisplayLanguage::japanese) ==
                   ":",
           "the question draws no prompt before Vim's words (the : line keeps its colon)");
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the asked match frame needs the software target");
        return;
    }
    win32::BodyTextLayouts layouts;
    auto settings = core::default_editor_settings();
    settings.wrapping = core::LineWrapping::no_wrap;
    const nenenib::application::TextLayoutRequest request{
        "a a a", settings, core::TextViewGeometry{layout_width, line_height, 96}};
    const auto layout = layouts.get(request);
    if (!layout.has_value())
    {
        unmeasured("the asked match frame needs a DirectWrite body layout");
        return;
    }
    check_frame(target, layouts, layout.value().Get());
}
} // namespace nenenib::tests::ui
