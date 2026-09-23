#pragma once

#include "VimRegisterTarget.hpp"

namespace nenenib::core
{
// `"{name}` で選んだレジスタ（ADR 0048 の決定 2）。name は named のとき小文字の a〜z で、
// `"A`〜`"Z` は同じ名前に append が立つ。unnamed と black_hole の name は `"` と `_`。
// 命令が完了すると vim_resting_from が捨てる。
struct VimRegisterSelection
{
    VimRegisterTarget target;
    char name;
    bool append;
};
} // namespace nenenib::core
