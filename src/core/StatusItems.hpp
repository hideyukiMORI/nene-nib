#pragma once

#include "DisplayText.hpp"

#include <array>
#include <cstddef>

namespace nenenib::core
{
// ステータスバー右側の 3 項目（行と桁・文字コード・改行）。
// 文字コードと改行は判別が入るまで固定（docs/design/2026-09-15-look.md 第 4 節）。
inline constexpr std::size_t status_item_count = 3;

[[nodiscard]] std::array<DisplayText, status_item_count> status_items_for(std::size_t line,
                                                                          std::size_t column);
} // namespace nenenib::core
