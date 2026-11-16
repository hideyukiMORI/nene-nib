#pragma once

#include <cstdint>

namespace nenenib::core
{
// 空の本文を engine がどう読むか（ADR 0188 の決定 3 (c)）。no_lines は Vim の ML_EMPTY（0 バイトの
// 文書・全部の行を消した後）で、`:d` は消すものが無い。one_line は eol の文書 `\n` の最後の改行を
// 除いた本文で、空の行が 1 つある（`:d` はレジスタに `\n` を書く）。本文が空でなければ読まない。
// 値が増えたら switch が落ちる（CPP-002）。
enum class VimEmptyText : std::uint8_t
{
    no_lines,
    one_line
};
} // namespace nenenib::core
