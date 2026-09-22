#pragma once

#include "VimBlockWidth.hpp"
#include "VimRegisterKind.hpp"

#include <optional>
#include <string>

namespace nenenib::core
{
// 無名レジスタ（ADR 0015 の決定 3）。本文は LF だけで、行単位なら末尾に改行が付く。
// Vim のレジスタは「行の列」であって文書ではないので、改行の形（CRLF / LF）は持たない。
// 文書の改行に直すのは VimInsertAt を写す controller の 1 か所（ARC-009）。
struct VimRegister
{
    std::string text;
    VimRegisterKind kind;
    // 矩形のときだけ持つ幅（ADR 0035 の決定 5）。短い行へ貼るときの空白の埋め方は本文だけでは
    // 決まらないので、固定 Vim も `getregtype()` で幅を返す。
    std::optional<VimBlockWidth> width = std::nullopt;
};
} // namespace nenenib::core
