#pragma once

#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspSymbol.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 記号の答えで読む記号の上限（ADR 0191 の決定 5・参照と同じ 1,000）。超えた分は捨てる。
inline constexpr std::size_t lsp_symbol_limit = 1000;

// `textDocument/documentSymbol` の答え（ADR 0191 の決定 3・5）。階層の `DocumentSymbol[]`（子は
// 親の名前つきで、親の直後に文書の順で平らにする）と平らな `SymbolInformation[]`（`location` を
// 持つ項目）の両方を受け、null は空の列。名前か位置の欠けた記号は捨てる（その子も読まない）。
// 再帰しない（深さは手元の積みで歩く）。OS に触れない純関数。
[[nodiscard]] std::expected<std::vector<LspSymbol>, LspAnswerFailure>
lsp_document_symbols(const JsonValue &result);

// 一覧の候補に添える種類の印（`SymbolKind` の名前の小文字・LSP の用語なので訳さない）。知らない
// 値は "symbol"。
[[nodiscard]] std::string_view lsp_symbol_kind_mark(std::uint64_t kind) noexcept;
} // namespace nenenib::core
