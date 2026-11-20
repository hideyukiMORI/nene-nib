// 端数のずらし（#1898）を載せた本文の 1 行の画素。core::scrolled_body の物理画素だけ上へずらした
// 行を、製品の BodyTextPainter（保持した字形の道）が本文の枠で切って描くと、(1) 同じ原点と clip の
// DrawTextLayout と 1 画素も違わず、(2) 端数の無い絵をずらしの画素だけ上へ送った絵と同じで、
// (3) 本文の上端より上（タブの帯の側）へはみ出さないことを、ソフトウェアの描画先で確かめる。
// 本物の DirectWrite の結果はフォントに依るので環境依存（QLT-013）。
#include "BodyTextLayouts.hpp"
#include "BodyTextPainter.hpp"
#include "SoftwareTarget.hpp"
#include "WheelTurn.hpp"
#include "WindowChecks.hpp"

#include "BodyLayout.hpp"
#include "DisplayForm.hpp"
#include "EditorSettings.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "RenderFailure.hpp"
#include "ScrollFraction.hpp"
#include "TextLayoutRequest.hpp"
#include "TextViewGeometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pixels = SoftwareTarget::Pixels;

[[nodiscard]] D2D1_RECT_F clip_of(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

[[nodiscard]] bool lit(const Pixels &pixels, std::size_t x, std::size_t y)
{
    const auto index = ((y * SoftwareTarget::width) + x) * 4;
    return pixels.at(index) != 0 || pixels.at(index + 1) != 0 || pixels.at(index + 2) != 0;
}

[[nodiscard]] bool dark_above(const Pixels &pixels, std::int32_t top)
{
    const auto count = static_cast<std::size_t>(top) * SoftwareTarget::width;
    for (std::size_t at = 0; at < count; ++at)
    {
        if (lit(pixels, at % SoftwareTarget::width, at / SoftwareTarget::width))
        {
            return false;
        }
    }
    return true;
}

// shifted の (x, y) が plain の (x, y + by) と同じ（本文の枠の中で、送った絵が届く所まで）。
[[nodiscard]] bool moved_up(const Pixels &plain, const Pixels &shifted, std::int32_t top,
                            std::int32_t by)
{
    const auto first = static_cast<std::size_t>(top) * SoftwareTarget::width;
    const auto last = static_cast<std::size_t>(SoftwareTarget::height - static_cast<UINT>(by)) *
                      SoftwareTarget::width;
    for (std::size_t at = first; at < last; ++at)
    {
        const auto x = at % SoftwareTarget::width;
        const auto y = at / SoftwareTarget::width;
        if (lit(plain, x, y + static_cast<std::size_t>(by)) != lit(shifted, x, y))
        {
            return false;
        }
    }
    return true;
}

// 窓の scroll_body が送る量（WHEEL_DELTA の単位の delta × 1 刻みの行数・#1898）。
void verify_wheel_amounts()
{
    using nenenib::ui::win32::wheel_scrolled;
    const auto slight = wheel_scrolled(-15, 3);
    expect(slight.lines == 0 && slight.parts == 45, "a small delta downward is a part of a row");
    const auto notch = wheel_scrolled(WHEEL_DELTA, 3);
    expect(notch.lines == -3 && notch.parts == 0, "a whole notch upward is whole rows");
    const auto mixed = wheel_scrolled(-100, 3);
    expect(mixed.lines == 2 && mixed.parts == 60, "a delta splits into rows and a part");
    const auto none = wheel_scrolled(-15, 0);
    expect(none.lines == 0 && none.parts == 0, "a zero setting does not scroll");
}

void verify_shifted_line(SoftwareTarget &target, win32::BodyTextLayouts &layouts)
{
    const auto settings = core::default_editor_settings();
    const auto body = core::body_layout_in(
        core::LayoutRect{0, 0, static_cast<std::int32_t>(SoftwareTarget::width),
                         static_cast<std::int32_t>(SoftwareTarget::height)},
        96, settings.font_size);
    const core::TextViewGeometry geometry{core::width_of(body.content), body.line_height, 96};
    const auto made = layouts.get(application::TextLayoutRequest{
        "Fine scroll: gjpqy WWW 日本語", settings, geometry, core::DisplayForm::text});
    if (!made)
    {
        unmeasured("the shifted body line needs a DirectWrite layout");
        return;
    }
    const auto drawn = [&](std::size_t fraction)
    {
        const auto scrolled = core::scrolled_body(body, fraction);
        const core::LayoutRect area{body.content.left, core::body_row_top(scrolled, 0),
                                    body.content.right, core::body_row_top(scrolled, 1)};
        std::optional<win32::RenderFailure> failure;
        const auto painted = target.painted(
            [&](ID2D1RenderTarget &context, ID2D1SolidColorBrush *brush)
            {
                win32::BodyTextPainter<ID2D1RenderTarget> painter(context, brush, layouts, failure);
                painter.set_rows(body.content.top, body.content.bottom);
                context.PushAxisAlignedClip(clip_of(body.content), D2D1_ANTIALIAS_MODE_ALIASED);
                painter.draw_body_text(made.value().Get(), area, D2D1_DRAW_TEXT_OPTIONS_CLIP);
                context.PopAxisAlignedClip();
            });
        const auto expected = target.clipped_layout(
            made.value().Get(),
            D2D1::Point2F(static_cast<float>(area.left), static_cast<float>(area.top)),
            clip_of(body.content));
        expect(!painted.empty() && painted == expected,
               "a shifted line draws the same pixels as DrawTextLayout in the body clip");
        return std::pair{painted, scrolled.scrolled};
    };
    const auto [plain, none] = drawn(0);
    expect(none == 0 && dark_above(plain, body.content.top),
           "a line without a fraction sits below the body top");
    for (const std::size_t fraction : std::array<std::size_t, 3>{1, core::scroll_fraction_units / 2,
                                                                 core::scroll_fraction_units - 1})
    {
        const auto [shifted, by] = drawn(fraction);
        expect(by > 0 || fraction * static_cast<std::size_t>(body.line_height) <
                             core::scroll_fraction_units,
               "the shift is the fraction of the line height in whole pixels");
        expect(dark_above(shifted, body.content.top),
               "a shifted line is cut at the body top and does not reach the tab band");
        expect(moved_up(plain, shifted, body.content.top, by),
               "a shifted line is the unshifted picture moved up by the shift");
    }
}
} // namespace

void verify_fine_scroll_paint()
{
    verify_wheel_amounts();
    SoftwareTarget target;
    if (!target.ready())
    {
        unmeasured("the shifted body line needs a software target");
        return;
    }
    win32::BodyTextLayouts layouts;
    verify_shifted_line(target, layouts);
}
} // namespace nenenib::tests::ui
