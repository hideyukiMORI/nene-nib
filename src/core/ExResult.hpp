#pragma once

#include "DisplayText.hpp"
#include "EditorSettings.hpp"
#include "ExFailure.hpp"

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

[[nodiscard]] std::expected<ExResult, ExFailure>
evaluate_ex(std::string_view text, const EditorSettings &settings, Appearance system_appearance);
[[nodiscard]] std::vector<std::string> command_completions(std::string_view prefix);
} // namespace nenenib::core
