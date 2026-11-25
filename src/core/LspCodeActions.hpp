#pragma once

#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspCodeAction.hpp"
#include "LspCommand.hpp"

#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 1 つの答えから読むクイックフィックスの項目の上限（#2005・値はこの 1 か所）。超えた分は捨てる
// （Ctrl+P の一覧に数百行を並べない）。
inline constexpr std::size_t lsp_code_action_limit = 100;

// サーバーから届く編集の要求の method（#2005・ADR 0194 の決定 6）。
inline constexpr std::string_view lsp_apply_edit_method = "workspace/applyEdit";

// `textDocument/codeAction` の答え `(Command | CodeAction)[] | null`（#2005・ADR 0194 の決定
// 9・10）。 null は空の列。項目はサーバーの順のまま。`command` の欄が文字列の項目は `Command` の
// literal で、 title と command（と arguments）を読み、command にも自分を置く。ほかは `CodeAction`
// で、title・ kind・isPreferred・edit（object のときだけ）・command（`Command`
// の形のときだけ）・data の有無を 読む。title の無い項目・`disabled` の項目（Nib は
// `disabledSupport` を名乗らないが届いても選べない ので捨てる）・edit も command も data
// も無い項目は 1 件ずつ捨てる。lsp_code_action_limit で切る。 配列でも null でもなければ
// invalid_result。OS に触れない純関数。
[[nodiscard]] std::expected<std::vector<LspCodeAction>, LspAnswerFailure>
lsp_code_actions(const JsonValue &result);

// `codeAction/resolve` の答え（`CodeAction` 1 つ・#2005・決定 10）。読みは lsp_code_actions の 1
// 件と 同じで、読めなければ invalid_result。
[[nodiscard]] std::expected<LspCodeAction, LspAnswerFailure>
lsp_code_action(const JsonValue &result);

// `workspace/executeCommand` の params（`ExecuteCommandParams`・command と、あれば arguments）。
[[nodiscard]] JsonValue lsp_execute_command_params(const LspCommand &command);
} // namespace nenenib::core
