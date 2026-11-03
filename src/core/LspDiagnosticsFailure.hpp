#pragma once

#include <cstdint>

namespace nenenib::core
{
// lsp_published_diagnostics が断る値の閉じた一覧（ADR 0161 の決定 2・5）。1 件ずつの診断の
// 不正（range や message の欠け・未知の severity）は断らずにその 1 件を捨てる。
enum class LspDiagnosticsFailure : std::uint8_t
{
    // 通知の method が `textDocument/publishDiagnostics` でない
    not_publish_diagnostics,
    // params が object でない・`uri` が文字列でない・`diagnostics` が配列でない・`version` が
    // null でも 0 以上の整数でもない
    invalid_params
};
} // namespace nenenib::core
