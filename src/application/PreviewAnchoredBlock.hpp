#pragma once

#include "MarkdownDocument.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// 枠の上端に掛かる錨の要素（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894 のレビュー M1）。block は
// ui の答えが運んだ要素の番号で、同じ文書（PreviewView.document の指す先）の間だけ持ち、文書が
// 替わると捨てる。持っていて文書の中ならそれ、無ければ anchor（本文のバイト位置）から
// core::preview_anchor_block で引く。同じ source_begin の要素が並ぶ所（水平線・空の項目）で、
// バイト位置からは後ろの要素しか引けないので、番号を優先する。錨の要素を読む所はここの 1 本。
[[nodiscard]] std::size_t preview_anchored_block(const core::MarkdownDocument &document,
                                                 core::Offset anchor,
                                                 std::optional<std::size_t> block);
} // namespace nenenib::application
