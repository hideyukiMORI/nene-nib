#include "PreviewTableScroll.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace nenenib::core
{
bool preview_table_wide(PreviewTableSpan span) noexcept
{
    return span.view > 0 && span.table > span.view;
}

std::int32_t preview_table_scroll_limit(PreviewTableSpan span) noexcept
{
    return preview_table_wide(span) ? span.table - span.view : 0;
}

std::int32_t preview_table_scroll_clamped(std::int32_t offset, PreviewTableSpan span) noexcept
{
    return std::clamp(offset, 0, preview_table_scroll_limit(span));
}

PreviewTableThumb preview_table_thumb(std::int32_t offset, PreviewTableSpan span) noexcept
{
    if (!preview_table_wide(span))
    {
        return PreviewTableThumb{0, std::max(span.view, 0)};
    }
    const auto proportional = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(span.view) * span.view + span.table / 2) / span.table);
    const auto length = std::min(std::max(proportional, preview_table_thumb_minimum), span.view);
    const auto room = static_cast<std::int64_t>(span.view - length);
    const auto limit = static_cast<std::int64_t>(preview_table_scroll_limit(span));
    const auto left = static_cast<std::int32_t>(
        (room * preview_table_scroll_clamped(offset, span) + limit / 2) / limit);
    return PreviewTableThumb{left, length};
}

PreviewTableBarPart preview_table_bar_part(std::int32_t offset, std::int32_t x,
                                           PreviewTableSpan span) noexcept
{
    const auto thumb = preview_table_thumb(offset, span);
    if (x < thumb.left)
    {
        return PreviewTableBarPart::before;
    }
    return x < thumb.left + thumb.length ? PreviewTableBarPart::thumb : PreviewTableBarPart::after;
}

std::int32_t preview_table_scroll_paged(std::int32_t offset, PreviewTableBarPart part,
                                        PreviewTableSpan span) noexcept
{
    const auto from = static_cast<std::int64_t>(preview_table_scroll_clamped(offset, span));
    const auto page = static_cast<std::int64_t>(std::max(span.view, 0));
    switch (part)
    {
    case PreviewTableBarPart::before:
        return static_cast<std::int32_t>(std::max<std::int64_t>(from - page, 0));
    case PreviewTableBarPart::thumb:
        return static_cast<std::int32_t>(from);
    case PreviewTableBarPart::after:
        return static_cast<std::int32_t>(
            std::min<std::int64_t>(from + page, preview_table_scroll_limit(span)));
    }
    std::unreachable();
}

std::int32_t preview_table_scroll_dragged(std::int32_t from, std::int32_t moved,
                                          PreviewTableSpan span) noexcept
{
    const auto thumb = preview_table_thumb(from, span);
    const auto room = static_cast<std::int64_t>(span.view - thumb.length);
    if (room <= 0)
    {
        return preview_table_scroll_clamped(from, span);
    }
    const auto limit = static_cast<std::int64_t>(preview_table_scroll_limit(span));
    const auto shift = static_cast<std::int64_t>(moved) * limit;
    // 0 へ向けて丸めず、近い整数へ（つまみを戻せば元の送りへ戻る）。
    const auto scaled = (shift >= 0 ? shift + room / 2 : shift - room / 2) / room;
    const auto next = static_cast<std::int64_t>(preview_table_scroll_clamped(from, span)) + scaled;
    return static_cast<std::int32_t>(
        std::clamp<std::int64_t>(next, 0, preview_table_scroll_limit(span)));
}
} // namespace nenenib::core
