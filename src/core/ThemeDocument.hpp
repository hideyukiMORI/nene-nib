#pragma once

#include "OwnedThemeSource.hpp"
#include "Theme.hpp"
#include "ThemeName.hpp"

namespace nenenib::core
{
// ファイル由来の文字列はこの値が所有する。Themeのviewは所有値より長生きさせない（ADR 0024）。
struct ThemeDocument
{
    ThemeName name;
    Appearance appearance;
    Palette ui;
    SyntaxPalette body;
    OwnedThemeSource source;
};

[[nodiscard]] Theme theme_view(const ThemeDocument &document) noexcept;
Theme theme_view(ThemeDocument &&document) = delete;
Theme theme_view(const ThemeDocument &&document) = delete;
} // namespace nenenib::core
