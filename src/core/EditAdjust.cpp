#include "EditAdjust.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::size_t line_feeds_in(std::string_view text) noexcept
{
    return static_cast<std::size_t>(std::ranges::count(text, '\n'));
}
} // namespace

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

LineNumber line_after_edit(LineNumber line, LineNumber edit_line, const Edit &edit) noexcept
{
    const std::size_t removed = line_feeds_in(edit.removed);
    if (line.value <= edit_line.value)
    {
        return line;
    }
    if (line.value <= edit_line.value + removed)
    {
        return edit_line;
    }
    return LineNumber{line.value - removed + line_feeds_in(edit.inserted)};
}
} // namespace nenenib::core
