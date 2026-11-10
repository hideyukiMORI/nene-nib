#pragma once

#include "MarkdownImage.hpp"
#include "PreviewBlock.hpp"

#include <vector>

namespace nenenib::core
{
// parse_markdown の結果。leaf ブロックの平らな列で、本文の順に並ぶ（ADR 0151 の決定 7）。images は
// 画像の節の本文の順の列（ADR 0155 の追補 47・block は blocks の番号で、前の節より小さくならない
// ）。
struct MarkdownDocument
{
    std::vector<PreviewBlock> blocks;
    std::vector<MarkdownImage> images = {};
};
} // namespace nenenib::core
