#pragma once

#include "VimTextObject.hpp"

namespace nenenib::core
{
// テキストオブジェクトの鍵 → 種類の表の 1 行（CPP-012 / ADR 0031 の決定 1）。
struct VimTextObjectBinding
{
    char32_t key;
    VimTextObject object;
};
} // namespace nenenib::core
