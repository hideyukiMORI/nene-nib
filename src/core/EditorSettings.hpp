#pragma once

#include "Appearance.hpp"
#include "DisplayText.hpp"
#include "FontSize.hpp"
#include "GuideVisibility.hpp"
#include "LineWrapping.hpp"
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
    GuideVisibility guide;
    LineWrapping wrapping = LineWrapping::wrap;
};

[[nodiscard]] EditorSettings default_editor_settings();
[[nodiscard]] bool same_settings(const EditorSettings &left, const EditorSettings &right) noexcept;
// 字の大きさか字体が変わると本文の行高と文字組みが変わる。窓は新しい行高の幾何を送り直す
// （ADR 0128 の決定 7）。判定はこの 1 本で、呼び手で欄を数えない。
[[nodiscard]] bool affects_line_height(const EditorSettings &before,
                                       const EditorSettings &after) noexcept;
[[nodiscard]] Theme selected_theme(const EditorSettings &settings,
                                   Appearance system_appearance) noexcept;
Theme selected_theme(EditorSettings &&, Appearance) = delete;
Theme selected_theme(const EditorSettings &&, Appearance) = delete;
} // namespace nenenib::core
