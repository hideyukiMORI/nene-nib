#pragma once

#include "FormattedSpan.hpp"
#include "JsonValue.hpp"
#include "LspAnswerFailure.hpp"
#include "LspPositionEncoding.hpp"
#include "LspTextEdit.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace nenenib::core
{
// `textDocument/formatting` と `textDocument/rangeFormatting` の答え（ADR 0191 の決定 3・7）。
// `TextEdit[]` を届いた順に読み、null は空の列。range か newText の欠けた項目が 1 つでもあれば
// 答え全体を読めないものとする（一部だけ当てると本文が崩れる）。OS に触れない純関数。
[[nodiscard]] std::expected<std::vector<LspTextEdit>, LspAnswerFailure>
lsp_text_edits(const JsonValue &result);

// 編集の列 → 全部を覆う 1 区間の置き換え（決定 7・1 つの undo の単位にするための形）。各 range は
// lsp_range_to_offsets で text の上へ写し、始まりの順（同じ始まりは届いた順・LSP の規則）に並べて
// 間の本文を挟んで組む。写せない range・重なる編集・空の列は値なし。caret は当てる前のキャレットを
// 写した位置: 手前で終わる編集の長さの差だけずらし、編集の中にあればその編集の新しい本文の終わり
// へ置く（同じ位置への挿入はキャレットの後ろに入る）。pieces は並べた編集の列。純関数。
[[nodiscard]] std::optional<FormattedSpan> formatted_span(const TextBuffer &text,
                                                          std::span<const LspTextEdit> edits,
                                                          LspPositionEncoding encoding,
                                                          Offset caret);
} // namespace nenenib::core
