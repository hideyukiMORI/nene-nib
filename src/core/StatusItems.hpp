#pragma once

#include "DisplayText.hpp"
#include "LineEnding.hpp"
#include "TextPosition.hpp"

#include <array>
#include <cstddef>

namespace nenenib::core
{
// ステータスバー右側の 3 項目（行と桁・文字コード・改行）。
// 文字コードは UTF-8 固定（ADR 0009 の決定 8）。改行は本文が持つ形をそのまま出す。
inline constexpr std::size_t status_item_count = 3;

[[nodiscard]] std::array<DisplayText, status_item_count> status_items_for(const TextPosition &caret,
                                                                          LineEnding ending);
} // namespace nenenib::core
