#include "PaletteMarks.hpp"

#include "UiTexts.hpp"

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

DisplayText palette_mark_hint(DisplayLanguage language)
{
    std::string hint;
    for (const PaletteMark &mark : palette_marks)
    {
        if (!hint.empty())
        {
            hint += ui_text(UiMessage::palette_mark_separator, language);
        }
        hint += mark.mark;
        hint += ' ';
        hint += ui_text(mark.name, language);
    }
    return DisplayText::parse(hint).value();
}

std::array<std::size_t, palette_marks.size()> palette_mark_hint_starts(DisplayLanguage language)
{
    // palette_mark_hint と同じ並べ方の長さを足していく（組 = 記号 1 バイト ＋ 空白 1 バイト ＋
    // 名前）。
    const auto separator = ui_text(UiMessage::palette_mark_separator, language).size();
    std::array<std::size_t, palette_marks.size()> starts{};
    std::size_t at = 0;
    for (std::size_t index = 0; index < palette_marks.size(); ++index)
    {
        if (index > 0)
        {
            at += separator;
        }
        starts.at(index) = at;
        at += 2 + ui_text(palette_marks.at(index).name, language).size();
    }
    return starts;
}
} // namespace nenenib::core
