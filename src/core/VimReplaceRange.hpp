#pragma once

#include "EditBoundary.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// rの置換（ADR 0029）。本文とcaretはLF換算。controllerが文書の改行へ直し、1 Editにする。
// 履歴の境界は engine が決める。NORMAL と VISUAL の置換は切り、回数付き INSERT の Esc が鍵の列を
// 流し直した置き換えは INSERT の 1 単位に吸収する（#1093・VimInsertAt と同じ形）。
struct VimReplaceRange
{
    OffsetRange range;
    std::string utf8;
    Offset caret;
    EditBoundary boundary;
};
} // namespace nenenib::core
