#pragma once

#include "MarkdownHeading.hpp"
#include "MarkdownImage.hpp"
#include "PreviewBlock.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
// parse_markdown の結果。leaf ブロックの平らな列で、本文の順に並ぶ（ADR 0151 の決定 7）。images は
// 画像の節の本文の順の列（ADR 0155 の追補 47・block は blocks の番号で、前の節より小さくならない
// ）。headings は見出しの要素ごとの名札の本文の順の列（ADR 0151 の追補 2026-11-19 の段 L3・#1869・
// 作るのは解析の 1 回の heading_slugs だけ）。
struct MarkdownDocument
{
    std::vector<PreviewBlock> blocks;
    std::vector<MarkdownImage> images = {};
    std::vector<MarkdownHeading> headings = {};
    // 表の数（表の行の PreviewTableRow::table は 0 から tables 未満・#1883・D107）。
    std::size_t tables = 0;
};
} // namespace nenenib::core
