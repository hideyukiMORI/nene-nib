#pragma once

#include "VimCharacterSearchKind.hpp"

namespace nenenib::core
{
// 完了した行内文字検索の記憶。本文位置を持たず、; / , が再利用する種別と対象だけを持つ。
struct VimCharacterSearch
{
    VimCharacterSearchKind kind;
    char32_t target;
};
} // namespace nenenib::core
