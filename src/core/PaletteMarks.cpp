#include "PaletteMarks.hpp"

#include "UiTexts.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// 行を案内に出すか（閉じた 3 値・CPP-002）。
[[nodiscard]] bool hinted(const PaletteMark &mark, bool language_server) noexcept
{
    switch (mark.hint)
    {
    case PaletteMarkHint::always:
        return true;
    case PaletteMarkHint::language_server:
        return language_server;
    case PaletteMarkHint::never:
        return false;
    }
    std::unreachable();
}
} // namespace

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

std::vector<PaletteMark> palette_hinted_marks(bool language_server)
{
    std::vector<PaletteMark> marks;
    std::ranges::copy_if(palette_marks, std::back_inserter(marks),
                         [language_server](const auto &mark)
                         { return hinted(mark, language_server); });
    return marks;
}

DisplayText palette_mark_hint(DisplayLanguage language, bool language_server)
{
    std::string hint;
    for (const PaletteMark &mark : palette_hinted_marks(language_server))
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

std::vector<std::size_t> palette_mark_hint_starts(DisplayLanguage language, bool language_server)
{
    // palette_mark_hint と同じ並べ方の長さを足していく（組 = 記号 1 バイト ＋ 空白 1 バイト ＋
    // 名前）。
    const auto separator = ui_text(UiMessage::palette_mark_separator, language).size();
    std::vector<std::size_t> starts;
    std::size_t at = 0;
    for (const PaletteMark &mark : palette_hinted_marks(language_server))
    {
        if (!starts.empty())
        {
            at += separator;
        }
        starts.push_back(at);
        at += 2 + ui_text(mark.name, language).size();
    }
    return starts;
}
} // namespace nenenib::core
