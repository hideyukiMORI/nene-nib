#pragma once

#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// 矩形の編集の 1 行ぶん（ADR 0035 の決定 3）。本文の `range` を `utf8` で置き換える。
// 本文も caret も LF 換算で、文書の改行へ直すのは controller の 1 か所（ARC-009）。
struct VimBlockEdit
{
    OffsetRange range;
    std::string utf8;
};
} // namespace nenenib::core
