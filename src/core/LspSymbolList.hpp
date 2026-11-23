#pragma once

#include "LspSymbol.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
// 読んだ記号の列（ADR 0191 の決定 5）。symbols は上限 lsp_symbol_limit までの文書の順、total は
// 切る前に読めた記号の数（切ったときの 1 行の知らせに使う・#1958 のレビュー L1）。公開 aggregate
// （CPP-003）。
struct LspSymbolList
{
    std::vector<LspSymbol> symbols;
    std::size_t total;
};
} // namespace nenenib::core
