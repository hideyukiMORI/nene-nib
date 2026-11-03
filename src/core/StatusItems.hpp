#pragma once

#include "DiagnosticCounts.hpp"
#include "DisplayText.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"

#include <array>
#include <cstddef>
#include <optional>

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

// ステータスバーの右端（言語の右）の 5 つ目の項目「E2 W1」（ADR 0161 の決定 4）。error だけなら
// 「E2」、warning だけなら「W1」、診断を上限で打ち切っていれば末尾に「+」。error も warning も 0
// なら 値なし（項目を出さず幅も取らない・status_bar_layout）。文字列はこの 1 本で作り、renderer に
// 書かない。
[[nodiscard]] std::optional<DisplayText> diagnostic_count_item(const DiagnosticCounts &counts);
} // namespace nenenib::core
