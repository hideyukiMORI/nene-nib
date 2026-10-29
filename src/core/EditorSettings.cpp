#include "EditorSettings.hpp"

#include "BuiltinThemes.hpp"

namespace nenenib::core
{
bool same_settings(const EditorSettings &left, const EditorSettings &right) noexcept
{
    return left.font_size.points() == right.font_size.points() &&
           left.font_family.text() == right.font_family.text() && left.theme == right.theme &&
           left.guide == right.guide && left.wrapping == right.wrapping &&
           left.ui_language == right.ui_language && left.lsp == right.lsp &&
           left.explorer == right.explorer && left.root == right.root &&
           left.features == right.features && left.lsp_commands == right.lsp_commands &&
           left.maps == right.maps && left.mapleader == right.mapleader &&
           left.ignorecase == right.ignorecase && left.smartcase == right.smartcase &&
           left.relativenumber == right.relativenumber && left.scrolloff == right.scrolloff;
}

VimPatternCase search_case(const EditorSettings &settings) noexcept
{
    if (!settings.ignorecase)
    {
        return VimPatternCase::sensitive;
    }
    return settings.smartcase ? VimPatternCase::smart : VimPatternCase::ignore;
}

std::optional<std::size_t> scrolloff_of(std::string_view text) noexcept
{
    constexpr std::size_t digits_limit = 3;
    constexpr std::size_t decimal_base = 10;
    if (text.empty() || text.size() > digits_limit || (text.size() > 1 && text.front() == '0'))
    {
        return std::nullopt;
    }
    std::size_t value = 0;
    for (const char digit : text)
    {
        if (digit < '0' || digit > '9')
        {
            return std::nullopt;
        }
        value = value * decimal_base + static_cast<std::size_t>(digit - '0');
    }
    return value;
}

bool affects_line_height(const EditorSettings &before, const EditorSettings &after) noexcept
{
    return before.font_size.points() != after.font_size.points() ||
           before.font_family.text() != after.font_family.text();
}

EditorSettings default_editor_settings()
{
    return EditorSettings{default_font_size(), DisplayText::parse("Cascadia Code").value(),
                          std::nullopt, GuideVisibility::shown};
}

Theme selected_theme(const EditorSettings &settings, Appearance system_appearance) noexcept
{
    if (settings.theme.has_value())
    {
        return settings.theme.value().view();
    }
    return theme_of(theme_for(system_appearance));
}
} // namespace nenenib::core
