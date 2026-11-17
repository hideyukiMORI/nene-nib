#include "TabTipLayout.hpp"

#include "DevicePixels.hpp"
#include "RgbColor.hpp"
#include "TabTip.hpp"
#include "TabTipMeasure.hpp"
#include "Utf16.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
using TextLayout = TabTipLayout::TextLayout;

// 測るための layout の幅。書式は折り返さないので、文の幅はこの値に依らない。
constexpr float measuring_width = 100'000.0F;

// layout の 1 行の幅（末尾の空白を含む）。測れなければ測りの幅（収まらない側）。
[[nodiscard]] float written_width(IDWriteTextLayout *layout) noexcept
{
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout->GetMetrics(&metrics)))
    {
        return measuring_width;
    }
    return metrics.widthIncludingTrailingWhitespace;
}

[[nodiscard]] TextLayout made(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                              std::string_view text)
{
    const auto wide = core::to_utf16(text);
    TextLayout layout;
    if (!wide.has_value() || FAILED(dwrite.CreateTextLayout(
                                 wide.value().c_str(), static_cast<UINT32>(wide.value().size()),
                                 format, measuring_width, measuring_width, &layout)))
    {
        return nullptr;
    }
    return layout;
}

// 上限に収まる最も少ない「真ん中で落とす文字の数」の layout（落とす数が多いほど狭い・二分探索）。
// 全部落としても収まらなければ「…」だけ（札は文を切り抜いて描く）。
[[nodiscard]] TextLayout fewest_dropped(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                        std::string_view text, std::int32_t limit)
{
    // 落とした文が上限に収まるか。layout を作れなければ値なし。
    const auto fits = [&](std::size_t dropped) -> std::optional<bool>
    {
        const TextLayout tried = made(dwrite, format, core::middle_clipped(text, dropped));
        if (!tried)
        {
            return std::nullopt;
        }
        return written_width(tried.Get()) <= static_cast<float>(limit);
    };
    std::size_t low = 1;
    std::size_t high = core::tab_tip_characters(text);
    while (low < high)
    {
        const std::size_t middle = low + ((high - low) / 2);
        const auto fitting = fits(middle);
        if (!fitting.has_value())
        {
            return nullptr;
        }
        high = fitting.value() ? middle : high;
        low = fitting.value() ? low : middle + 1;
    }
    return made(dwrite, format, core::middle_clipped(text, low));
}
// トークンの色を Direct2D の色へ（不透明）。
[[nodiscard]] D2D1_COLOR_F color_of(core::RgbColor color) noexcept
{
    constexpr float full = 255.0F;
    return D2D1::ColorF(static_cast<float>(color.red) / full,
                        static_cast<float>(color.green) / full,
                        static_cast<float>(color.blue) / full);
}
} // namespace

IDWriteTextLayout *TabTipLayout::fitted(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                        std::string_view text, std::int32_t limit)
{
    if (layout_ && format_ == format && limit_ == limit && text_ == text)
    {
        return layout_.Get();
    }
    clear();
    TextLayout whole = made(dwrite, format, text);
    if (whole && written_width(whole.Get()) > static_cast<float>(limit))
    {
        whole = fewest_dropped(dwrite, format, text, limit);
    }
    if (!whole)
    {
        return nullptr;
    }
    layout_ = whole;
    text_ = std::string(text);
    limit_ = limit;
    format_ = format;
    return layout_.Get();
}

std::optional<TabTipPaint> TabTipLayout::painted(const core::TitleBarLayout &layout,
                                                 std::size_t tab, const core::Palette &palette,
                                                 std::uint32_t dpi) const
{
    DWRITE_TEXT_METRICS metrics{};
    if (!layout_ || FAILED(layout_->GetMetrics(&metrics)))
    {
        return std::nullopt;
    }
    const auto box = core::tab_tip_rect(
        layout, tab,
        core::TabTipMeasure{
            static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace)),
            static_cast<std::int32_t>(std::ceil(metrics.height)), dpi});
    const float scale = static_cast<float>(dpi) / static_cast<float>(core::reference_dpi);
    return TabTipPaint{
        D2D1::RectF(static_cast<float>(box.left), static_cast<float>(box.top),
                    static_cast<float>(box.right), static_cast<float>(box.bottom)),
        layout_.Get(),
        D2D1::Point2F(
            static_cast<float>(box.left + core::to_pixels(core::tab_tip_padding_x_dips, dpi)),
            static_cast<float>(box.top + core::to_pixels(core::tab_tip_padding_y_dips, dpi))),
        static_cast<float>(core::tab_tip_radius_dips) * scale,
        scale,
        color_of(palette.panel),
        color_of(palette.panel_border),
        color_of(palette.text)};
}

void TabTipLayout::clear() noexcept
{
    layout_.Reset();
    text_.clear();
    limit_ = 0;
    format_ = nullptr;
}
} // namespace nenenib::ui::win32
