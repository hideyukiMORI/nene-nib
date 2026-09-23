#pragma once

#include "VimKey.hpp"

#include <vector>

namespace nenenib::application
{
// マクロのレジスタへ鍵の列を置く（ADR 0046 の決定 6）。Vim の `:let @a = "…"` に当たる入口で、
// 置くだけで何も実行しない。name の a〜z は置き換え、A〜Z は追記、ほかの名前は何もしない。
// 何を置くかを決めるのは core の vim_macro_stored である（ARC-004）。
struct StoreVimMacro
{
    char32_t name;
    std::vector<core::VimKey> keys;
};
} // namespace nenenib::application
