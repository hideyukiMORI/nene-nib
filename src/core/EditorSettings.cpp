#include "EditorSettings.hpp"

#include "BuiltinThemes.hpp"

namespace nenenib::core
{
bool same_settings(const EditorSettings &left, const EditorSettings &right) noexcept
{
    return left.font_size.points() == right.font_size.points() &&
           left.font_family.text() == right.font_family.text() && left.theme == right.theme;
}

EditorSettings default_editor_settings()
{
    return EditorSettings{default_font_size(), DisplayText::parse("Cascadia Code").value(),
                          std::nullopt};
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
