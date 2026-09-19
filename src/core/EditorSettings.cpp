#include "EditorSettings.hpp"

#include "BuiltinThemes.hpp"

namespace nenenib::core
{
EditorSettings default_editor_settings()
{
    return EditorSettings{default_font_size(), DisplayText::parse("Cascadia Code").value(),
                          std::nullopt};
}

const Theme &selected_theme(const EditorSettings &settings, Appearance system_appearance) noexcept
{
    if (settings.theme.has_value())
    {
        return theme_of(settings.theme.value());
    }
    return theme_of(theme_for(system_appearance));
}
} // namespace nenenib::core
