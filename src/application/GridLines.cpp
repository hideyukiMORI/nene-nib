#include "GridLines.hpp"

#include <algorithm>

namespace nenenib::application
{
namespace
{
constexpr std::size_t kept_lines = 512;
} // namespace

std::shared_ptr<const core::GridLine>
GridLines::line(const core::TextBuffer &text, core::LineNumber number, std::size_t columns)
{
    const auto found = std::ranges::find_if(
        lines_, [number, columns](const auto &entry)
        { return entry.first == number.value && entry.second->columns() == columns; });
    if (found != lines_.end())
    {
        return found->second;
    }
    if (lines_.size() >= kept_lines)
    {
        lines_.erase(lines_.begin());
    }
    lines_.emplace_back(number.value, std::make_shared<const core::GridLine>(
                                          core::GridLine::build(text.line_text(number), columns)));
    return lines_.back().second;
}
} // namespace nenenib::application
