#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
// Explicit accepted inventory; deliberately independent of the production table.
inline constexpr std::array<std::string_view, 95> expected_theme_names{{
    "ubuntu-aubergine",
    "neutral-light",
    "solarized-dark",
    "solarized-light",
    "monokai",
    "dracula",
    "one-dark",
    "night-owl",
    "night-owl-light",
    "blue",
    "catppuccin",
    "darkblue",
    "default",
    "delek",
    "desert",
    "elflord",
    "evening",
    "habamax",
    "industry",
    "koehler",
    "lunaperche",
    "morning",
    "murphy",
    "novum",
    "pablo",
    "peachpuff",
    "quiet",
    "retrobox",
    "ron",
    "shine",
    "slate",
    "sorbet",
    "torte",
    "unokai",
    "wildcharm",
    "zaibatsu",
    "zellner",
    "abyss",
    "vscode-dark-2026",
    "vscode-light-2026",
    "vscode-dark-modern",
    "vscode-dark-plus",
    "vscode-dark",
    "vscode-high-contrast",
    "vscode-high-contrast-light",
    "vscode-light-modern",
    "vscode-light-plus",
    "vscode-light",
    "kimbie-dark",
    "monokai-dimmed",
    "quiet-light",
    "red",
    "tomorrow-night-blue",
    "monalisa",
    "catppuccin-latte",
    "catppuccin-frappe",
    "catppuccin-macchiato",
    "nord",
    "gruvbox",
    "gruvbox-light",
    "tokyo-night",
    "tokyo-night-storm",
    "tokyo-night-moon",
    "tokyo-night-light",
    "github-dark",
    "github-light",
    "github-dark-dimmed",
    "one-light",
    "intellij-light",
    "darcula",
    "rose-pine",
    "rose-pine-moon",
    "rose-pine-dawn",
    "everforest-dark",
    "everforest-light",
    "kanagawa",
    "kanagawa-dragon",
    "kanagawa-lotus",
    "material-oceanic",
    "material-darker",
    "material-lighter",
    "material-palenight",
    "material-deep-ocean",
    "ayu-dark",
    "ayu-mirage",
    "ayu-light",
    "cobalt2",
    "synthwave-84",
    "alloy-core",
    "verdant-rage",
    "scarlet-web",
    "relic-sand",
    "soap-rebellion",
    "code-rain",
    "arcane-library",
}};

inline std::vector<std::string> expected_theme_commands()
{
    std::vector<std::string> result;
    for (const auto name : expected_theme_names)
    {
        result.push_back("colorscheme " + std::string{name});
    }
    result.emplace_back("colorscheme system");
    return result;
}
} // namespace nenenib::tests
