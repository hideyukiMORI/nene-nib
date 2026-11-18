#pragma once

#include "LspRange.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nenenib::core
{
// `textDocument/completion` の答えの項目 1 つ（ADR 0190 の決定 4）。label は面に出す名前、kind は
// LSP の `CompletionItemKind`（無ければ値なし）、detail は無ければ空。sort_text は並べる鍵
// （`sortText`・無ければ label）、filter_text は絞り込む鍵（`filterText`・無ければ label）。
// insert_text は確定で入れる平文（`textEdit.newText`・`insertText`・label の順・スニペット形式の
// 項目は `$0` `${1:x}` の印を外した形）。edit_range は `textEdit` の範囲（`InsertReplaceEdit` は
// insert の範囲・無ければ値なし・位置は要求を出した版の本文の上）。公開 aggregate（CPP-003）。
struct LspCompletionItem
{
    std::string label;
    std::optional<std::uint64_t> kind;
    std::string detail;
    std::string sort_text;
    std::string filter_text;
    std::string insert_text;
    std::optional<LspRange> edit_range;
};
} // namespace nenenib::core
