#pragma once

#include "Appearance.hpp"
#include "DisplayText.hpp"
#include "FontSize.hpp"
#include "Theme.hpp"
#include "ThemeChoice.hpp"

#include <optional>

namespace nenenib::core
{
struct EditorSettings
{
    FontSize font_size;
    DisplayText font_family;
    // 無しはsystem。選択済みのテーマは生存する配色を共有する（ADR 0025）。
    std::optional<ThemeChoice> theme;
};

[[nodiscard]] EditorSettings default_editor_settings();
[[nodiscard]] bool same_settings(const EditorSettings &left, const EditorSettings &right) noexcept;
[[nodiscard]] Theme selected_theme(const EditorSettings &settings,
                                   Appearance system_appearance) noexcept;
Theme selected_theme(EditorSettings &&, Appearance) = delete;
Theme selected_theme(const EditorSettings &&, Appearance) = delete;
} // namespace nenenib::core
