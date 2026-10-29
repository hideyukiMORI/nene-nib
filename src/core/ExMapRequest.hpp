#pragma once

#include "ExMapVerb.hpp"
#include "VimModeSet.hpp"

#include <string>

namespace nenenib::core
{
// Vim の `map` の Ex の要求（ADR 0176 の決定 6・ExClipboardRequest と同じく controller が写す）。
// 左辺と右辺は鍵の記法の文字列のまま（検査は controller が呼ぶ map_edited の map_row_of）。
// leader では rhs が新しい `mapleader` の値で、modes と lhs は使わない。remap が偽なら noremap。
struct ExMapRequest
{
    ExMapVerb verb;
    VimModeSet modes;
    std::string lhs;
    std::string rhs;
    bool remap = true;
};
} // namespace nenenib::core
