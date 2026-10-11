#include "TextLayoutPoint.hpp"

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] bool trailing(core::CaretAffinity affinity) noexcept
{
    switch (affinity)
    {
    case core::CaretAffinity::leading:
        return false;
    case core::CaretAffinity::trailing:
        return true;
    }
    std::unreachable();
}
} // namespace

std::expected<TextLayoutPoint, application::TextLayoutFailure>
text_layout_point(IDWriteTextLayout *layout, UINT32 units, core::CaretAffinity affinity)
{
    const bool after = trailing(affinity) && units > 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    TextLayoutPoint point{};
    if (FAILED(layout->HitTestTextPosition(units - (after ? 1U : 0U), after ? TRUE : FALSE,
                                           &point.x, &point.y, &metrics)))
    {
        return std::unexpected(application::TextLayoutFailure::unavailable);
    }
    return point;
}
} // namespace nenenib::ui::win32
