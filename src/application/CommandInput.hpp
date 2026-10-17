#pragma once

#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "FontFamilies.hpp"
#include "InputLinePrompt.hpp"
#include "InputLineView.hpp"
#include "SearchLine.hpp"

#include <variant>

namespace nenenib::application
{
// 同時に開ける入力は一つ。本文のモードとは独立した閉じた和型（ADR 0023 / ADR 0032）。
using CommandInput = std::variant<core::CommandLine, core::CommandPalette, core::SearchLine>;

// 入力行の見え方（ADR 0032 の決定 1）。どの選択肢もこの 1 つの値に畳んでから描く。
[[nodiscard]] core::InputLineView input_line_of(const CommandInput &input);
// 入力行の種類（プロンプト）だけ。input_line_of(input).prompt と同じ値を、本文を写さずに返す。
[[nodiscard]] core::InputLinePrompt input_line_prompt_of(const CommandInput &input);
[[nodiscard]] std::expected<CommandInput, core::ExFailure>
inserted_command(const CommandInput &input, std::string_view text);
[[nodiscard]] CommandInput edited_command(const CommandInput &input, core::CommandEdit edit);
// 入力が字体の名前の補完を求めているか（Ex の行と Ctrl+P の面だけ・検索は求めない・#597）。
[[nodiscard]] bool wants_font_families(const CommandInput &input);
// 同じ入力に字体の一覧を入れた値（検索の行はそのまま）。
[[nodiscard]] CommandInput with_font_families(const CommandInput &input,
                                              const core::FontFamilies &fonts);
} // namespace nenenib::application
