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

std::string palette_input_for(const PaletteQuery &query)
{
    const auto mark = std::ranges::find(palette_marks, query.scope, &PaletteMark::scope);
    if (mark != palette_marks.end())
    {
        return std::string(1, mark->mark) + std::string(query.query);
    }
    const bool escape = palette_query_of(query.query).scope != PaletteScope::files;
    return (escape ? " " : "") + std::string(query.query);
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
