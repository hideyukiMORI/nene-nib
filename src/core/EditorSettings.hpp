#pragma once

#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "DisplayText.hpp"
#include "FontSize.hpp"
#include "Theme.hpp"

#include <optional>

namespace nenenib::core
{
struct EditorSettings
{
    FontSize font_size;
    DisplayText font_family;
    // 無しは system。テーマ名の解析は BuiltinThemes の唯一の表を使う。
    std::optional<BuiltinTheme> theme;
};

[[nodiscard]] EditorSettings default_editor_settings();
[[nodiscard]] bool same_settings(const EditorSettings &left, const EditorSettings &right) noexcept;
[[nodiscard]] const Theme &selected_theme(const EditorSettings &settings,
                                          Appearance system_appearance) noexcept;
} // namespace nenenib::core
