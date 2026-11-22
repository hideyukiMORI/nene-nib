#pragma once

#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspSignature.hpp"

#include <cstddef>
#include <expected>
#include <optional>

namespace nenenib::core
{
// 札に出す署名の文の上限（バイト・コードポイントを割らない）。超えた分は捨てる（強める範囲も中へ
// 切る）。
inline constexpr std::size_t lsp_signature_label_limit = 1024;

// `textDocument/signatureHelp` の答え（ADR 0191 の決定 3・6）。`activeSignature`（無い・範囲の外は
// 0）の `label` と、今の引数（署名の `activeParameter` が先・無ければ全体の `activeParameter`）の
// `ParameterInformation.label` の範囲。範囲は文字列なら label の中の最初の位置、`[start, end]` なら
// UTF-16 の位置（バイトへ写す）。null・署名が無い答えは値なし、形の違う答えは invalid_result。
// OS に触れない純関数。
[[nodiscard]] std::expected<std::optional<LspSignature>, LspAnswerFailure>
lsp_signature_help(const JsonValue &result);
} // namespace nenenib::core
