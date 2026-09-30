#include "PaletteMarks.hpp"

#include <algorithm>
#include <string>

namespace nenenib::core
{
PaletteQuery palette_query_of(std::string_view input) noexcept
{
    if (input.empty())
    {
        return PaletteQuery{PaletteScope::files, input};
    }
    const auto found = std::ranges::find(palette_marks, input.front(), &PaletteMark::mark);
    if (found == palette_marks.end())
    {
        return PaletteQuery{PaletteScope::files, input};
    }
    return PaletteQuery{found->scope, input.substr(1)};
}

DisplayText palette_mark_hint()
{
    std::string hint;
    for (const PaletteMark &mark : palette_marks)
    {
        if (!hint.empty())
        {
            hint += "　";
        }
        hint += mark.mark;
        hint += ' ';
        hint += mark.name;
    }
    return DisplayText::parse(hint).value();
}
} // namespace nenenib::core
