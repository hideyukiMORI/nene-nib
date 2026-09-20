#pragma once

#include "VimRegisterKind.hpp"

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
};
} // namespace nenenib::core
