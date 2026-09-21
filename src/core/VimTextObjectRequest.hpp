#pragma once

#include "VimTextObject.hpp"
#include "VimTextObjectScope.hpp"

namespace nenenib::core
{
// 「どのオブジェクトを、内側か周りか」の 1 組（ADR 0031 の決定 2）。範囲関数へ 1 つの値で渡す。
struct VimTextObjectRequest
{
    VimTextObjectScope scope;
    VimTextObject object;
};
} // namespace nenenib::core
