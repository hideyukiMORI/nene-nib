#pragma once

#include "CommandLine.hpp"
#include "CommandPalette.hpp"

#include <variant>

namespace nenenib::application
{
// 同時に開ける入力は一つ。本文のモードとは独立した閉じた和型（ADR 0023）。
using CommandInput = std::variant<core::CommandLine, core::CommandPalette>;

[[nodiscard]] const core::CommandLine &command_line_of(const CommandInput &input);
[[nodiscard]] std::expected<CommandInput, core::ExFailure>
inserted_command(const CommandInput &input, std::string_view text);
[[nodiscard]] CommandInput edited_command(const CommandInput &input, core::CommandEdit edit);
} // namespace nenenib::application
