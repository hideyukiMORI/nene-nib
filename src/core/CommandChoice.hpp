#pragma once

#include "CommandChoiceKind.hpp"
#include "CommandLine.hpp"
#include "DisplayText.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
struct CommandChoice
{
    DisplayText label;
    std::string command;
    CommandChoiceKind kind;
};

[[nodiscard]] std::vector<CommandChoice> palette_choices(const CommandLine &input);
} // namespace nenenib::core
