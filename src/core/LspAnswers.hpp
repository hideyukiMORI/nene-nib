#pragma once

#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspCompletionList.hpp"
#include "LspLocation.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// 補完の答えで読む項目の上限（ADR 0190 の決定 4・限界）。超えた分は捨てる。
inline constexpr std::size_t lsp_completion_limit = 500;
// hover の平文の上限（決定 4）。行の数とバイトの数の両方で切る。
inline constexpr std::size_t lsp_hover_line_limit = 40;
inline constexpr std::size_t lsp_hover_byte_limit = 4096;

// サーバーの答えを読む 3 本（ADR 0190 の決定 4）。引数は答えの `result`。OS に触れない純関数で、
// 位置（LspRange）は写さずに返す（本文の位置へは使う側が lsp_range_to_offsets で写す）。

// `textDocument/completion` の答え。`CompletionItem[]` と `CompletionList{isIncomplete, items}` の
// 両方を受け、null は空の列。label の無い項目は捨てる。`textEdit` は `TextEdit` の range と
// `InsertReplaceEdit` の insert の範囲を読み、形の崩れた `textEdit` は無いものとして扱う。
// `additionalTextEdits`・`command`・`data` は読まない（限界）。
[[nodiscard]] std::expected<LspCompletionList, LspAnswerFailure>
lsp_completion_items(const JsonValue &result);

// `textDocument/hover` の答え → 平文。`contents` の `MarkupContent`（plaintext と markdown）・
// `MarkedString`（文字列と `{language, value}`）・その配列を受け、配列は改行でつなぐ。markdown の
// 囲みの ``` の行は外す（ほかの記法はそのまま）。改行は LF にそろえ、前後の空行を落とし、
// lsp_hover_line_limit 行と lsp_hover_byte_limit バイト（コードポイントを割らない）で切る。null・
// 形の違う答え・空の答えは値なし。
[[nodiscard]] std::optional<std::string> lsp_hover_text(const JsonValue &result);

// `textDocument/definition` の答え。`Location`・`Location[]`・`LocationLink[]`（targetUri と
// targetSelectionRange・無ければ targetRange）を受け、null は空の列。uri や range の欠けた
// 場所は捨てる。
[[nodiscard]] std::expected<std::vector<LspLocation>, LspAnswerFailure>
lsp_locations(const JsonValue &result);
} // namespace nenenib::core
