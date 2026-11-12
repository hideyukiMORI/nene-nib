#pragma once

#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "FontFamilies.hpp"
#include "InputHistory.hpp"
#include "InputLinePrompt.hpp"
#include "InputLineView.hpp"
#include "SearchLine.hpp"

#include <optional>
#include <string>
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
// 入力があふれているか（上限 core::input_line_limit を超えた・差し込みを捨てた・#1568）。`:` の
// 行・検索の行・面の入力のどれも同じ InputText の規則で、あふれた入力の確定は丸ごと断る。
[[nodiscard]] bool overflows_command(const CommandInput &input);
// 履歴の上下（#725・ADR 0022 決定 2 の追補）。`:` の行は ex、検索の行は search の履歴を歩き、
// 面は変えない（面の上下は候補の選択・ADR 0150 の決定 2）。
[[nodiscard]] CommandInput recalled_command(const CommandInput &input, const core::InputHistory &ex,
                                            const core::InputHistory &search,
                                            core::CommandEdit direction);
// 上下が失敗しないか（Vim の c_<Up> c_<Down> は履歴を動けないと beep_flush・#1528）。`:` の行と
// 検索の行は recalled_command が 1 歩動くか。履歴を歩かない「行: 」「置換後: 」と面は Vim に無い
// 入力行なので失敗にしない（true）。
[[nodiscard]] bool recalls_command(const CommandInput &input, const core::InputHistory &ex,
                                   const core::InputHistory &search, core::CommandEdit direction);
// 入力が字体の名前の補完を求めているか（Ex の行と Ctrl+P の面だけ・検索は求めない・#597）。
[[nodiscard]] bool wants_font_families(const CommandInput &input);
// 焦点の候補の命令（ADR 0150 の決定 4）。面では選択した候補が実行の候補ならその命令、`:` の行
// では巡回で選ばれている候補（入力欄の全文）。検索の行・巡回していない `:` の行・ファイルや
// 操作の候補は無し。
[[nodiscard]] std::optional<std::string> focused_command_of(const CommandInput &input);
// 同じ入力に字体の一覧を入れた値（検索の行はそのまま）。
[[nodiscard]] CommandInput with_font_families(const CommandInput &input,
                                              const core::FontFamilies &fonts);
} // namespace nenenib::application
