#pragma once

#include "JsonValue.hpp"
#include "LspCommand.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// クイックフィックスの答えの 1 項目（#2005・ADR 0194 の決定 9・10）。`Command` の literal と
// `CodeAction` の両方をこの形に読む。title は題名、kind は
// `CodeActionKind`（無ければ空・訳さない）、 preferred は `isPreferred`、edit は `WorkspaceEdit` の
// JSON のまま（当てるときに lsp_workspace_edit で読む・無ければ値なし）、command は edit の後に送る
// `Command`（無ければ値なし・ `Command` の literal は自分自身）、resolvable は `data` を持つ（edit
// が無ければ `codeAction/resolve` で引く）、source は届いた項目の JSON のまま（resolve の
// params）。 公開 aggregate（CPP-003）。
struct LspCodeAction
{
    std::string title;
    std::string kind;
    bool preferred;
    std::optional<JsonValue> edit;
    std::optional<LspCommand> command;
    bool resolvable;
    JsonValue source;
};
} // namespace nenenib::core
