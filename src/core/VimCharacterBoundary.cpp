#include "VimCharacterBoundary.hpp"

#include "DisplayWidth.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

namespace nenenib::core
{
// 行頭（0）はどの code point でも文字の先頭。
bool vim_character_continues(std::string_view utf8, Offset at) noexcept
{
    return at.value > 0 && at.value < utf8.size() &&
           display_width(code_point_at(utf8, at)) == DisplayWidth::zero;
}

Offset vim_character_end(std::string_view utf8, Offset at) noexcept
{
    Offset end = next_code_point(utf8, at);
    while (vim_character_continues(utf8, end))
    {
        end = next_code_point(utf8, end);
    }
    return end;
}

Offset vim_character_start(std::string_view utf8, Offset at) noexcept
{
    Offset start = vim_character_continues(utf8, at) ? at : previous_code_point(utf8, at);
    while (vim_character_continues(utf8, start))
    {
        start = previous_code_point(utf8, start);
    }
    return start;
}
} // namespace nenenib::core
