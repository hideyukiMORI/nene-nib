#pragma once

#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "InputLineView.hpp"
#include "SearchLine.hpp"

#include <variant>

namespace nenenib::application
{
// 同時に開ける入力は一つ。本文のモードとは独立した閉じた和型（ADR 0023 / ADR 0032）。
using CommandInput = std::variant<core::CommandLine, core::CommandPalette, core::SearchLine>;

// 入力行の見え方（ADR 0032 の決定 1）。どの選択肢もこの 1 つの値に畳んでから描く。
[[nodiscard]] core::InputLineView input_line_of(const CommandInput &input);
[[nodiscard]] std::expected<CommandInput, core::ExFailure>
inserted_command(const CommandInput &input, std::string_view text);
[[nodiscard]] CommandInput edited_command(const CommandInput &input, core::CommandEdit edit);
} // namespace nenenib::application
