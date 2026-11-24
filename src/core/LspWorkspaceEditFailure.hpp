#pragma once

#include <cstdint>

namespace nenenib::core
{
// `WorkspaceEdit` の読み（lsp_workspace_edit）が答え全体を断る理由の閉じた一覧（ADR 0194 の決定 4・
// ARC-010・CPP-005）。1 つでも当たれば 1 つも当てない。
enum class LspWorkspaceEditFailure : std::uint8_t
{
    // object でも null でもない・欄の形が違う・range か newText の欠けた編集がある。
    malformed,
    // `documentChanges` に作る・名前を変える・消すの項目（`kind` を持つ項目）がある（Nib は
    // `resourceOperations` を名乗らない）。
    resource_operation,
    // `needsConfirmation: true` の注釈を指す編集がある（確かめの問いを持たない・決定 3）。
    needs_confirmation,
    // ファイルの数が lsp_workspace_edit_file_limit を超える。
    too_many_files
};
} // namespace nenenib::core
