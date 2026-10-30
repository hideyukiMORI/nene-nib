#include "SubstituteAll.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "VimSearch.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 1 行の長さのある当たりを ranges の末尾へ足す（vim_line_matches の 1 本）。位置は本文のバイト。
void append_line_ranges(std::vector<OffsetRange> &ranges, const TextBuffer &text,
                        const VimPattern &pattern, LineNumber line)
{
    const Offset start = text.line_start(line);
    for (const auto &found : vim_line_matches(text.line_text(line), pattern))
    {
        if (found.begin.value < found.end.value)
        {
            ranges.push_back(OffsetRange{Offset{start.value + found.begin.value},
                                         Offset{start.value + found.end.value}});
        }
    }
}

// 本文の長さのある当たりを前から順に。
[[nodiscard]] std::vector<OffsetRange> matched_ranges(const TextBuffer &text,
                                                      const VimPattern &pattern)
{
    std::vector<OffsetRange> ranges;
    for (std::size_t line = 1; line <= text.line_count(); ++line)
    {
        append_line_ranges(ranges, text, pattern, LineNumber{line});
    }
    return ranges;
}
} // namespace

std::optional<Edit> substituted_all(const TextBuffer &text, const VimPattern &pattern,
                                    std::string_view replacement)
{
    const auto ranges = matched_ranges(text, pattern);
    if (ranges.empty())
    {
        return std::nullopt;
    }
    const Offset at = ranges.front().begin;
    std::string inserted;
    Offset copied = at;
    for (const auto &range : ranges)
    {
        inserted += text.text_range(copied, range.begin);
        inserted += replacement;
        copied = range.end;
    }
    return Edit{at, text.text_range(at, copied), std::move(inserted), at};
}
} // namespace nenenib::core
