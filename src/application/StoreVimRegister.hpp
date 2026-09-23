#pragma once

#include "VimRegister.hpp"

namespace nenenib::application
{
// 名前つきレジスタへ本文を置く（ADR 0048 の決定 6）。Vim の `:let @a = "…"` に当たる入口で、
// 置くだけで何も実行しない。name の a〜z は置き換え、A〜Z は追記、ほかの名前は何もしない。
// 何を置くかを決めるのは core の vim_register_stored である（ARC-004）。
struct StoreVimRegister
{
    char name;
    core::VimRegister value;
};
} // namespace nenenib::application
