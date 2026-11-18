#include "HoverTipLayout.hpp"

#include "HoverTip.hpp"
#include "TabTip.hpp"
#include "TabTipLayout.hpp"
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
using TextLayout = HoverTipLayout::TextLayout;

// layout の高さの上限。行の上限は画面行の数で守るので、高さでは切らない。
constexpr float unbounded_height = 100'000.0F;

// 幅 limit で折り返した layout（語が幅より長ければ語の途中で折る）。作れなければ null。
[[nodiscard]] TextLayout wrapped(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                 std::string_view text, std::int32_t limit)
{
    const auto wide = core::to_utf16(text);
    TextLayout layout;
    if (!wide.has_value() ||
        FAILED(dwrite.CreateTextLayout(wide.value().c_str(),
                                       static_cast<UINT32>(wide.value().size()), format,
                                       static_cast<float>(limit), unbounded_height, &layout)) ||
        FAILED(layout->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK)))
    {
        return nullptr;
    }
    return layout;
}

// 折り返した画面行の数。測れなければ値なし。
[[nodiscard]] std::optional<std::size_t> rows_of(IDWriteTextLayout *layout) noexcept
{
    DWRITE_TEXT_METRICS metrics{};
    if (layout == nullptr || FAILED(layout->GetMetrics(&metrics)))
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(metrics.lineCount);
}

// 画面行が上限に収まる最も多い「残す文字の数」の layout（残す数が少ないほど行が減る・二分探索）。
// 1 文字でも収まらなければ 1 文字と「…」（札は文を切り抜いて描く）。
[[nodiscard]] TextLayout most_kept(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                   std::string_view text, std::int32_t limit)
{
    std::size_t low = 1;
    std::size_t high = core::tab_tip_characters(text);
    while (low < high)
    {
        const std::size_t middle = low + ((high - low + 1) / 2);
        const TextLayout tried =
            wrapped(dwrite, format, core::hover_tip_clipped(text, middle), limit);
        const auto rows = rows_of(tried.Get());
        if (!rows.has_value())
        {
            return nullptr;
        }
        const bool fits = rows.value() <= core::hover_tip_max_lines;
        low = fits ? middle : low;
        high = fits ? high : middle - 1;
    }
    return wrapped(dwrite, format, core::hover_tip_clipped(text, low), limit);
}
} // namespace

IDWriteTextLayout *HoverTipLayout::fitted(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                                          std::string_view text, std::int32_t limit)
{
    if (layout_ && format_ == format && limit_ == limit && text_ == text)
    {
        return layout_.Get();
    }
    clear();
    TextLayout whole = wrapped(dwrite, format, text, limit);
    if (rows_of(whole.Get()).value_or(0) > core::hover_tip_max_lines)
    {
        whole = most_kept(dwrite, format, text, limit);
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

std::optional<TabTipPaint> HoverTipLayout::painted(const core::LayoutRect &caret,
                                                   const core::LayoutRect &area,
                                                   const core::Palette &palette,
                                                   std::uint32_t dpi) const
{
    DWRITE_TEXT_METRICS metrics{};
    if (!layout_ || FAILED(layout_->GetMetrics(&metrics)))
    {
        return std::nullopt;
    }
    const auto box = core::hover_tip_rect(
        caret, area,
        core::TabTipMeasure{
            static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace)),
            static_cast<std::int32_t>(std::ceil(metrics.height)), dpi});
    return tip_paint(box, layout_.Get(), palette, dpi);
}

void HoverTipLayout::clear() noexcept
{
    layout_.Reset();
    text_.clear();
    limit_ = 0;
    format_ = nullptr;
}
} // namespace nenenib::ui::win32
