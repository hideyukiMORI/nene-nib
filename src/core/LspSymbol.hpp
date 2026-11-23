#pragma once

#include "LspRange.hpp"

#include <cstdint>
#include <string>

namespace nenenib::core
{
// 記号の答えの 1 つ（ADR 0191 の決定 5）。name は名前、kind は LSP の `SymbolKind`（1〜26・知らない
// 値もそのまま）、parent は親の名前（階層の答えは親の記号の名前・平らな答えは `containerName`・
// 無ければ空）、at は跳ぶ先の範囲（`DocumentSymbol` の selectionRange・無ければ range、
// `SymbolInformation` の location.range）。位置は写さずに返す。公開 aggregate（CPP-003）。
struct LspSymbol
{
    std::string name;
    std::uint64_t kind;
    std::string parent;
    LspRange at;
};
} // namespace nenenib::core
