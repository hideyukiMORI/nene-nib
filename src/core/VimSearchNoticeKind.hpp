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
    // E385: Search hit BOTTOM without match for: <pattern>（'nowrapscan' の前向き・#1561）
    hit_bottom_without_match,
    // E384: Search hit TOP without match for: <pattern>（'nowrapscan' の後ろ向き・#1561）
    hit_top_without_match,
    // 未対応の構文の拒否（Vim には無い報せ）
    unsupported_pattern,
    // `:s` の族（ADR 0168 の決定 5）。N substitution(s) on M line(s)（置き換えが 3
    // つ以上のときだけ・ Vim の 'report' の既定 2）と、`n` の N match(es) on M
    // line(s)（いつも）。数は count と lines。
    substitutions,
    matches,
    // E33: No previous substitute regular expression（前の置換が無い `:s` `:&` `&` `:~`）
    no_previous_substitute,
    // E16: Invalid range（範囲が本文の外・ADR 0168 の決定 1）
    invalid_range,
    // E20: Mark not set（範囲のマークが無い）
    mark_not_set,
    // E493: Backwards range given（範囲が逆向き・Vim の :normal の中の答え）
    backwards_range,
    // E939: Positive count required（`e` の無い回数 0・`e` があれば Vim は黙って何もしない・実測）
    positive_count,
    // E363: pattern uses more memory than 'maxmempattern'（群の範囲を引く道の升の数の上限を超える
    // 当たり・ADR 0168 の追補 13。本文は変えない）
    pattern_too_complex
};
} // namespace nenenib::core
