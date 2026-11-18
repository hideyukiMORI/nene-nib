#pragma once

#include "LspCompletionItem.hpp"

#include <vector>

namespace nenenib::core
{
// `textDocument/completion` の答えを読んだ値（ADR 0190 の決定 4）。incomplete は `CompletionList`
// の `isIncomplete`（配列と null の答えは偽・上限で切ったら真）。items は届いた順のまま（並べ替えと
// 絞り込みは使う側）で、lsp_completion_limit 件で切る。公開 aggregate（CPP-003）。
struct LspCompletionList
{
    bool incomplete;
    std::vector<LspCompletionItem> items;
};
} // namespace nenenib::core
