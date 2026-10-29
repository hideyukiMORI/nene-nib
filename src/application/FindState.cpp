#include "FindState.hpp"

#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "VimMatchRequest.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"

#include <cstddef>
#include <string>

namespace nenenib::application
{
namespace
{
// at の行の中で、at を含む（contains）か at から始まる（!contains）長さのある一致。
[[nodiscard]] std::optional<core::OffsetRange> line_match(const core::TextBuffer &text,
                                                          const core::VimPattern &pattern,
                                                          core::Offset at, bool contains)
{
    const core::LineNumber line = text.position_of(at).line;
    const core::Offset start = text.line_start(line);
    const std::size_t column = at.value - start.value;
    const std::string bytes = text.line_text(line);
    for (const auto &found : core::vim_line_matches(bytes, pattern))
    {
        const bool hit = contains ? found.begin.value <= column && column < found.end.value
                                  : found.begin.value == column;
        if (hit && found.begin.value < found.end.value)
        {
            return core::OffsetRange{core::Offset{start.value + found.begin.value},
                                     core::Offset{start.value + found.end.value}};
        }
    }
    return std::nullopt;
}
} // namespace

std::optional<core::OffsetRange> current_find_match(const core::TextBuffer &text,
                                                    const core::VimPattern &pattern,
                                                    core::Offset caret)
{
    const auto containing = line_match(text, pattern, caret, true);
    if (containing.has_value())
    {
        return containing;
    }
    const auto hit = core::vim_find_match(
        text, pattern,
        core::VimMatchRequest{text.position_of(caret), core::VimSearchDirection::forward, 1});
    if (!hit)
    {
        return std::nullopt;
    }
    return line_match(text, pattern, text.offset_of(hit.value().position), false);
}
} // namespace nenenib::application
