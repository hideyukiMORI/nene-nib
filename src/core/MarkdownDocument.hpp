#pragma once

#include "PreviewBlock.hpp"

#include <vector>

namespace nenenib::core
{
// parse_markdown の結果。leaf ブロックの平らな列で、本文の順に並ぶ（ADR 0151 の決定 7）。
struct MarkdownDocument
{
    std::vector<PreviewBlock> blocks;
};
} // namespace nenenib::core
