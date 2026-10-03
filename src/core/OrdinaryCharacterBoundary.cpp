#include "OrdinaryCharacterBoundary.hpp"

#include "DisplayWidth.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::size_t point_bytes = 4;

[[nodiscard]] char32_t point_at(const TextBuffer &text, Offset at)
{
    return code_point_at(
        text.text_range(at, Offset{std::min(at.value + point_bytes, text.size_bytes())}),
        Offset{0});
}

[[nodiscard]] Offset point_before(const TextBuffer &text, Offset at)
{
    const Offset begin{at.value - std::min(at.value, point_bytes)};
    const std::string content = text.text_range(begin, at);
    return Offset{begin.value + previous_code_point(content, Offset{content.size()}).value};
}

[[nodiscard]] Offset point_after(const TextBuffer &text, Offset at)
{
    const std::string content =
        text.text_range(at, Offset{std::min(at.value + point_bytes, text.size_bytes())});
    return Offset{at.value + next_code_point(content, Offset{0}).value};
}

[[nodiscard]] bool variation_selector(char32_t value) noexcept
{
    return (value >= 0xFE00 && value <= 0xFE0F) || (value >= 0xE0100 && value <= 0xE01EF);
}

[[nodiscard]] bool control_point(char32_t value) noexcept
{
    return value <= 0x1F || value == 0x7F;
}

[[nodiscard]] bool continues_character(const TextBuffer &text, Offset at)
{
    if (at.value == 0 || at.value >= text.size_bytes() ||
        display_width(point_at(text, at)) != DisplayWidth::zero)
    {
        return false;
    }
    const char32_t previous = point_at(text, point_before(text, at));
    return !control_point(previous) && !variation_selector(previous);
}

[[nodiscard]] Offset preceding_atom(const TextBuffer &text, Offset at)
{
    Offset previous = point_before(text, at);
    if (text.line_ending() == LineEnding::crlf && point_at(text, previous) == U'\n' &&
        previous.value > 0 && point_at(text, point_before(text, previous)) == U'\r')
    {
        previous = point_before(text, previous);
    }
    return previous;
}

[[nodiscard]] Offset following_atom(const TextBuffer &text, Offset at)
{
    Offset next = point_after(text, at);
    if (text.line_ending() == LineEnding::crlf && point_at(text, at) == U'\r' &&
        point_at(text, next) == U'\n')
    {
        next = point_after(text, next);
    }
    return next;
}

[[nodiscard]] Offset character_floor(const TextBuffer &text, Offset at)
{
    while (continues_character(text, at))
    {
        at = point_before(text, at);
    }
    return at;
}

[[nodiscard]] Offset character_end(const TextBuffer &text, Offset at)
{
    Offset end = following_atom(text, at);
    while (continues_character(text, end))
    {
        end = point_after(text, end);
    }
    return end;
}

[[nodiscard]] Offset backward_deletion(const TextBuffer &text, Offset at)
{
    Offset start = preceding_atom(text, at);
    while (start.value > 0 && variation_selector(point_at(text, start)))
    {
        const Offset previous = point_before(text, start);
        if (control_point(point_at(text, previous)))
        {
            break;
        }
        start = previous;
    }
    return start;
}

[[nodiscard]] Offset forward_deletion(const TextBuffer &text, Offset at)
{
    Offset end = character_end(text, at);
    if (control_point(point_at(text, at)))
    {
        return end;
    }
    while (end.value < text.size_bytes() && variation_selector(point_at(text, end)))
    {
        end = point_after(text, end);
    }
    return end;
}
} // namespace

Offset ordinary_character_boundary(const TextBuffer &text, Offset at,
                                   OrdinaryCharacterAction action)
{
    switch (action)
    {
    case OrdinaryCharacterAction::previous:
        return character_floor(text, preceding_atom(text, at));
    case OrdinaryCharacterAction::next:
        return character_end(text, at);
    case OrdinaryCharacterAction::containing:
        return character_floor(text, at);
    case OrdinaryCharacterAction::backspace:
        return backward_deletion(text, at);
    case OrdinaryCharacterAction::erase_forward:
        return forward_deletion(text, at);
    }
    std::unreachable();
}
} // namespace nenenib::core
