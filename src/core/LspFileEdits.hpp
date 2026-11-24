#pragma once

#include "LspTextEdit.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// `WorkspaceEdit` の 1 ファイルぶんの編集（ADR 0194 の決定 4）。uri は答えの URI のまま（パスへの
// 写しは application の file_path_of_uri）、version は `TextDocumentEdit.textDocument.version`
// （`changes` の形と null は値なし）、edits は届いた順の `TextEdit` の列（同じ URI の項目が 2 つ
// 以上あれば届いた順につないだ列）。公開 aggregate（CPP-003）。
struct LspFileEdits
{
    std::string uri;
    std::optional<std::uint64_t> version;
    std::vector<LspTextEdit> edits;
};
} // namespace nenenib::core
