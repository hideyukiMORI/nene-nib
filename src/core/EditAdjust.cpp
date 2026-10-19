#include "EditAdjust.hpp"

#include <cstddef>

namespace nenenib::core
{
Offset offset_after_edit(Offset offset, const Edit &edit) noexcept
{
    const std::size_t at = edit.at.value;
    const std::size_t removed_end = at + edit.removed.size();
    if (offset.value < at)
    {
        return offset;
    }
    if (offset.value == at)
    {
        return Offset{at + edit.inserted.size()};
    }
    if (offset.value < removed_end)
    {
        return edit.at;
    }
    return Offset{offset.value - edit.removed.size() + edit.inserted.size()};
}
} // namespace nenenib::core
