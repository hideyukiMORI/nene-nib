#pragma once

#include <cstdint>

namespace nenenib::core
{
// 検索が出す報せの閉じた一覧（ADR 0032 の決定 5）。文言は vim_search_message 1 か所だけが持ち、
// Vim にある報せは Vim の文言そのまま、未対応構文の拒否だけが本実装の文言である。
enum class VimSearchNoticeKind : std::uint8_t
{
    // E486: Pattern not found: <pattern>
    pattern_not_found,
    // E35: No previous regular expression
    no_previous_pattern,
    // E348: No string under cursor（`*` / `#` の語が無い）
    no_word_under_cursor,
    // search hit BOTTOM, continuing at TOP
    wrapped_to_top,
    // search hit TOP, continuing at BOTTOM
    wrapped_to_bottom,
    // 未対応の構文の拒否（Vim には無い報せ）
    unsupported_pattern
};
} // namespace nenenib::core
