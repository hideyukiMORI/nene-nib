#pragma once

#include "DisplayText.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"

#include <array>
#include <cstddef>

namespace nenenib::core
{
// ステータスバー右側の 4 項目（行と桁・文字コード・改行・言語）。文字コードは読んだ形をそのまま
// 出し、改行も本文が持つ形をそのまま出す（ADR 0010 の決定 14）。言語は右端で、名前は
// language_name の表（ADR 0154 の決定 6）。
inline constexpr std::size_t status_item_count = 4;

[[nodiscard]] std::array<DisplayText, status_item_count> status_items_for(const TextPosition &caret,
                                                                          TextEncoding encoding,
                                                                          LineEnding ending,
                                                                          Language language);
} // namespace nenenib::core
