#pragma once

namespace nenenib::core
{
// Vim の大小の置き換え（`~` `g~` `gu` `gU` と VISUAL の `~` `u` `U`）の 1 code point の写像
// （Issue #781・ADR 0015 の決定 10 の追記）。表は Unicode の簡単な（1 対 1 の）写像だけの生成物
// CaseMappingTable.hpp の 1 つで、1 対多（`ß` → `SS`）・ロケール（トルコ語の i）・タイトルケースは
// 使わない。写像の無い code point はそのまま返す。
//
// 「小文字」は大文字の写像を持つ code point、「大文字」は小文字の写像を持つ code point で、両方を
// 持つタイトルケースの字（`ǅ`）は小文字として扱う（Vim の swapchar は MB_ISLOWER を先に見る）。
// だから小文字化は小文字（`ǅ` を含む）を変えない（固定 Vim 9.1 で `ǄǅǆΣ` の `v$u` は `ǆǅǆσ`）。
[[nodiscard]] char32_t vim_to_upper(char32_t value) noexcept;
[[nodiscard]] char32_t vim_to_lower(char32_t value) noexcept;

// 小文字なら大文字へ、そうでなく大文字なら小文字へ。どちらでもなければそのまま。
[[nodiscard]] char32_t vim_toggled(char32_t value) noexcept;
} // namespace nenenib::core
