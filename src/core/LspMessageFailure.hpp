#pragma once

#include <cstdint>

namespace nenenib::core
{
// lsp_incoming が断る値の閉じた一覧（JSON-RPC 2.0・ADR 0160 の決定 5）。
enum class LspMessageFailure : std::uint8_t
{
    // 本文が object でない（JSON-RPC の batch の配列も受けない・LSP は使わない）
    not_object,
    // `jsonrpc` が文字列の "2.0" でない
    not_json_rpc,
    // `method` が文字列でない
    invalid_method,
    // `id` が ±2^53 の内の整数でも文字列でもない（null を含む）
    invalid_id,
    // `method` も `id` も無い
    missing_id,
    // `error` が整数の `code` と文字列の `message` を持つ object でない
    invalid_error
};
} // namespace nenenib::core