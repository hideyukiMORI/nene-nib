#pragma once

#include "AutosaveId.hpp"
#include "FilePath.hpp"
#include "FormattedSpan.hpp"
#include "LspTextEdit.hpp"
#include "UnopenedFile.hpp"

#include <optional>
#include <variant>
#include <vector>

namespace nenenib::application
{
// 言語サーバーの編集（WorkspaceEdit）の 1 ファイルぶんの当て先（#2004・ADR 0194 の決定 5）。place
// は 開いているタブの退避番号か、タブの無いファイル。path は同じファイルの項目を 1 つにまとめる鍵
// （FilePort::same_file で比べる）、edits は届いた順の編集（同じファイルの項目はつなぐ）、span は
// 検査の 2 段目で組んだ 1 区間の置き換え（formatted_span・本文が変わらないファイルは列から外す）。
// 公開 aggregate（CPP-003）。
struct ServerEditFile
{
    std::variant<AutosaveId, UnopenedFile> place;
    core::FilePath path;
    std::vector<core::LspTextEdit> edits;
    std::optional<core::FormattedSpan> span = std::nullopt;
};
} // namespace nenenib::application
