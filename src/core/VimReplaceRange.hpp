#pragma once

#include "Offset.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// rの置換（ADR 0029）。本文とcaretはLF換算。controllerが文書の改行へ直し、1 Editにする。
struct VimReplaceRange
{
    OffsetRange range;
    std::string utf8;
    Offset caret;
};
} // namespace nenenib::core
