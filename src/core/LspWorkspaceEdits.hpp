#pragma once

#include "JsonValue.hpp"
#include "LspWorkspaceEdit.hpp"
#include "LspWorkspaceEditFailure.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::core
{
// 1 つの `WorkspaceEdit` で変えるファイルの数の上限（ADR 0194 の決定 4・hide に決めてほしいこと 2
// の仮の決め・値はこの 1 か所）。超える答えは 1 つも当てない。
inline constexpr std::size_t lsp_workspace_edit_file_limit = 32;

// `WorkspaceEdit` の答え（ADR 0194 の決定 4）。null は空の編集。`documentChanges` があればそれを
// 使い `changes` は捨てる（LSP の文）。`TextDocumentEdit` の version は null と欠けた欄が値なし、
// 0 以上の整数がその値。同じ URI の項目が 2 つ以上あれば届いた順につなぐ。`AnnotatedTextEdit` は
// `TextEdit` として読み（ファイルごとの列の読みは lsp_text_edits の 1 本）、`annotationId` が
// `needsConfirmation: true` の注釈を指せば needs_confirmation、`kind` を持つ項目（作る・名前を
// 変える・消す）があれば resource_operation、形の崩れは malformed、編集のあるファイルが
// lsp_workspace_edit_file_limit を超えれば too_many_files で、どれも答え全体を断る。編集の無い
// ファイルは列に入れない。OS に触れない純関数。
[[nodiscard]] std::expected<LspWorkspaceEdit, LspWorkspaceEditFailure>
lsp_workspace_edit(const JsonValue &result);
} // namespace nenenib::core
