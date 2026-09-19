#pragma once

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ThemeCatalog.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
struct ExResult
{
    std::optional<EditorSettings> settings;
    DisplayText message;
};

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
evaluate_ex(std::string_view text, const EditorSettings &settings, Appearance system_appearance,
            const ThemeCatalog &themes = ThemeCatalog::builtins());
[[nodiscard]] std::vector<std::string>
command_completions(std::string_view prefix, const ThemeCatalog &themes = ThemeCatalog::builtins());
[[nodiscard]] std::vector<std::string>
ex_command_candidates(const ThemeCatalog &themes = ThemeCatalog::builtins());
} // namespace nenenib::core
