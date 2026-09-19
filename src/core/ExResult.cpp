#include "ExResult.hpp"

#include "BuiltinThemes.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::string_view trimmed(std::string_view text)
{
    const auto first = text.find_first_not_of(' ');
    if (first == std::string_view::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

[[nodiscard]] DisplayText theme_message(const EditorSettings &settings, Appearance appearance)
{
    std::string message = "colorscheme=" + std::string(selected_theme(settings, appearance).name);
    if (!settings.theme.has_value())
    {
        message += " (system)";
    }
    return DisplayText::parse(message).value();
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> colorscheme(std::string_view name,
                                                                       EditorSettings settings,
                                                                       Appearance appearance,
                                                                       const ThemeCatalog &themes)
{
    if (name.empty())
    {
        return ExResult{std::nullopt, theme_message(settings, appearance)};
    }
    if (name == "system")
    {
        settings.theme = std::nullopt;
        return ExResult{settings, theme_message(settings, appearance)};
    }
    const auto parsed = ThemeName::parse(name);
    if (!parsed)
    {
        return std::unexpected(ExFailure::unknown_theme);
    }
    const auto found = themes.find(parsed.value());
    if (!found)
    {
        return std::unexpected(found.error());
    }
    settings.theme = found.value();
    return ExResult{settings, theme_message(settings, appearance)};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> set_font_size(std::string_view value,
                                                                         EditorSettings settings)
{
    const auto size = FontSize::parse(value);
    if (!size)
    {
        return std::unexpected(ExFailure::invalid_font_size);
    }
    settings.font_size = size.value();
    return ExResult{settings, DisplayText::parse("fontsize=" + std::string(value)).value()};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> set_gui_font(std::string_view value,
                                                                        EditorSettings settings)
{
    const auto separator = value.rfind(":h");
    if (separator == std::string_view::npos)
    {
        return std::unexpected(ExFailure::invalid_font_family);
    }
    const auto family = DisplayText::parse(trimmed(value.substr(0, separator)));
    if (!family)
    {
        return std::unexpected(ExFailure::invalid_font_family);
    }
    const auto size = FontSize::parse(value.substr(separator + 2));
    if (!size)
    {
        return std::unexpected(ExFailure::invalid_font_size);
    }
    settings.font_size = size.value();
    settings.font_family = family.value();
    return ExResult{settings, DisplayText::parse("guifont=" + std::string(value)).value()};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
set_option(std::string_view option, const EditorSettings &settings)
{
    if (option.starts_with("fontsize="))
    {
        return set_font_size(option.substr(9), settings);
    }
    if (option.starts_with("guifont="))
    {
        return set_gui_font(option.substr(8), settings);
    }
    return std::unexpected(ExFailure::unknown_option);
}
} // namespace

std::expected<ExResult, ExEvaluationFailure> evaluate_ex(std::string_view text,
                                                         const EditorSettings &settings,
                                                         Appearance system_appearance,
                                                         const ThemeCatalog &themes)
{
    if (text.size() > DisplayText::maximum_bytes)
    {
        return std::unexpected(ExFailure::too_long);
    }
    if (!validate_utf8(text) || has_control_character(text))
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    if (text.find('|') != std::string_view::npos)
    {
        return std::unexpected(ExFailure::unknown_command);
    }
    text = trimmed(text);
    const auto space = text.find(' ');
    const auto name = text.substr(0, space);
    const auto argument =
        space == std::string_view::npos ? std::string_view{} : trimmed(text.substr(space + 1));
    if (name == "colorscheme")
    {
        return colorscheme(argument, settings, system_appearance, themes);
    }
    if (name == "set")
    {
        return set_option(argument, settings);
    }
    return std::unexpected(ExFailure::unknown_command);
}

std::vector<std::string> ex_command_candidates(const ThemeCatalog &themes)
{
    std::vector<std::string> candidates{"colorscheme", "set fontsize=", "set guifont="};
    for (const auto &name : themes.names())
    {
        candidates.push_back("colorscheme " + name);
    }
    candidates.emplace_back("colorscheme system");
    return candidates;
}

std::vector<std::string> command_completions(std::string_view prefix, const ThemeCatalog &themes)
{
    auto candidates = ex_command_candidates(themes);
    std::erase_if(candidates,
                  [prefix](const std::string &candidate)
                  {
                      return !candidate.starts_with(prefix) ||
                             (candidate.starts_with("colorscheme ") &&
                              !prefix.starts_with("colorscheme "));
                  });
    return candidates;
}

DisplayText ex_failure_message(ExFailure failure)
{
    switch (failure)
    {
    case ExFailure::unknown_command:
        return DisplayText::parse("Unknown command").value();
    case ExFailure::unknown_option:
        return DisplayText::parse("Use set fontsize=<pt> or set guifont=<name>:h<pt>").value();
    case ExFailure::unknown_theme:
        return DisplayText::parse("Unknown colorscheme (Tab lists names)").value();
    case ExFailure::invalid_font_size:
        return DisplayText::parse("Font size must be 8 to 40 pt").value();
    case ExFailure::invalid_font_family:
        return DisplayText::parse("Use set guifont=<name>:h<pt>").value();
    case ExFailure::invalid_text:
        return DisplayText::parse("Command must be a single line of UTF-8 text").value();
    case ExFailure::too_long:
        return DisplayText::parse("Command is limited to 256 bytes").value();
    }
    std::unreachable();
}
} // namespace nenenib::core
