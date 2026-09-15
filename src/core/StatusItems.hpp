#pragma once

#include "DisplayText.hpp"
#include "LineEnding.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"

#include <array>
#include <cstddef>

namespace nenenib::core
{
// ステータスバー右側の 3 項目（行と桁・文字コード・改行）。文字コードは読んだ形をそのまま出し、
// 改行も本文が持つ形をそのまま出す（ADR 0010 の決定 14）。
inline constexpr std::size_t status_item_count = 3;

[[nodiscard]] std::array<DisplayText, status_item_count>
status_items_for(const TextPosition &caret, TextEncoding encoding, LineEnding ending);
} // namespace nenenib::core
