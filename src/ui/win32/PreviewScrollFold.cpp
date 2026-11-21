#include "PreviewScrollFold.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>

namespace nenenib::ui::win32
{
namespace
{
// 上へ畳む（nudge が負の間、前の要素の長さを足す）。要素 0 の上で止める。
[[nodiscard]] std::expected<PreviewScrollSpot, RenderFailure>
folded_up(PreviewScrollSpot spot, const PreviewSpanOf &span_of)
{
    while (spot.nudge < 0.0F && spot.block > 0)
    {
        --spot.block;
        const auto span = span_of(spot.block);
        if (!span)
        {
            return std::unexpected(span.error());
        }
        spot.nudge += span.value();
    }
    spot.nudge = std::max(spot.nudge, 0.0F);
    return spot;
}

// 下へ畳む（nudge が要素の長さ以上の間、引いて次の要素へ）。最後の要素では、その下端が枠の下端に
// 来るまで（要素が枠より低ければ上端が枠の上端まで）。
[[nodiscard]] std::expected<PreviewScrollSpot, RenderFailure>
folded_down(PreviewScrollSpot spot, std::size_t count, float height, const PreviewSpanOf &span_of)
{
    for (;;)
    {
        const auto span = span_of(spot.block);
        if (!span)
        {
            return std::unexpected(span.error());
        }
        if (spot.block + 1U >= count)
        {
            spot.nudge = std::min(spot.nudge, std::max(span.value() - height, 0.0F));
            return spot;
        }
        if (spot.nudge < span.value())
        {
            return spot;
        }
        spot.nudge -= span.value();
        ++spot.block;
    }
}
} // namespace

std::expected<PreviewScrollSpot, RenderFailure> preview_scroll_fold(PreviewScrollSpot from,
                                                                    std::size_t count, float height,
                                                                    const PreviewSpanOf &span_of)
{
    if (count == 0)
    {
        return from;
    }
    from.block = std::min(from.block, count - 1U);
    return from.nudge < 0.0F ? folded_up(from, span_of) : folded_down(from, count, height, span_of);
}
} // namespace nenenib::ui::win32
