#pragma once

#include "JsonValue.hpp"
#include "LspId.hpp"
#include "LspIncoming.hpp"
#include "LspMessageFailure.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
// JSON-RPC 2.0 の「メソッドが無い」の code（サーバーからの要求を断るときに返す）。
inline constexpr std::int64_t lsp_method_not_found_code = -32601;

// JSON-RPC の本文 ↔ 型の対応の唯一の層（ADR 0160 の決定 5）。OS に触れない純関数。
// 本文 1 通の値（parse_json の結果）を答え・通知・要求に分ける。`method` があれば要求か通知
// （`id` の有無で分ける）、無ければ `id` を持つ答え。`jsonrpc` は "2.0" でなければならない。
[[nodiscard]] std::expected<LspIncoming, LspMessageFailure> lsp_incoming(const JsonValue &message);

// Nib が送る要求の本文。id は application の通し番号。params が JsonNull なら `params` を書かない
// （`shutdown` など・JSON-RPC は params に null を許さない）。
[[nodiscard]] std::string lsp_request_body(std::int64_t id, std::string_view method,
                                           const JsonValue &params);

// Nib が送る通知の本文（`id` を持たない）。params の扱いは要求と同じ。
[[nodiscard]] std::string lsp_notification_body(std::string_view method, const JsonValue &params);

// サーバーからの要求を MethodNotFound（-32601）で断る答えの本文。
[[nodiscard]] std::string lsp_method_not_found_body(const LspId &id);
} // namespace nenenib::core
