#pragma once

#include "VimMode.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectScope.hpp"

namespace nenenib::core
{
// 「どのオブジェクトを、内側か周りか、どのモードから」の組（ADR 0031 の決定 2）。範囲関数へ 1
// つの値で 渡す。mode は段落と文だけが読む（VISUAL か・行単位 VISUAL かで広げ方が変わる・Issue #729
// で実測）。
struct VimTextObjectRequest
{
    VimTextObjectScope scope;
    VimTextObject object;
    VimMode mode;
};
} // namespace nenenib::core
