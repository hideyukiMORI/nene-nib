#pragma once

namespace nenenib::core
{
// Vim の大小の置き換え（`~` `g~` `gu` `gU` と VISUAL の `~` `u` `U`）の 1 code point の写像
// （Issue #781 / #990・ADR 0015 の決定 10 の追記と追補）。表は UnicodeData の簡単な（1 対 1 の）
// 写像だけの生成物 CaseMappingTable.hpp の 1 つで、ロケール（トルコ語の i）・タイトルケースは
// 使わない。1 対多の `gU` の `ß` → `SS` は表の外（VimStepOperators.cpp の cased_text）で、ここは
// `ß` をそのまま返す。写像の無い code point はそのまま返す。
//
// 「小文字」は大文字の写像を持つ code point、「大文字」は小文字の写像を持つ code point で、両方を
// 持つタイトルケースの字（`ǅ`）は小文字として扱う（Vim の swapchar は MB_ISLOWER を先に見る）。
// だから小文字化は小文字（`ǅ` を含む）を変えない（固定 Vim 9.1 で `ǄǅǆΣ` の `v$u` は `ǆǅǆσ`）。
[[nodiscard]] char32_t vim_to_upper(char32_t value) noexcept;
[[nodiscard]] char32_t vim_to_lower(char32_t value) noexcept;

// 小文字なら大文字へ、そうでなく大文字なら小文字へ。どちらでもなければそのまま。
[[nodiscard]] char32_t vim_toggled(char32_t value) noexcept;

// `ignorecase` の照合で比べる畳んだ形（ADR 0032 の追補・#1041）。Vim の utf_fold（Unicode の
// CaseFolding の C と S）に合わせ、大文字にしてから小文字にする（`ς` と `σ` と `Σ`、`ſ` と `s`、
// Kelvin の `K` と `k` が同じになる）。トルコ語の `İ` `ı` は CaseFolding の C / S に無く畳まない。
[[nodiscard]] char32_t vim_folded(char32_t value) noexcept;

// vim_folded で value と同じ形に畳まれる字（畳みの仲間）が、ASCII の大小だけを畳んだバイト列の
// 比較で value と見分けられない字だけか（#1003）。ASCII の英字は仲間が自分の大小 2 つだけのとき真
// （`k` `s` は Kelvin の `K`（U+212A）と `ſ`（U+017F）が仲間なので偽）、ASCII の外の字は仲間が
// 自分だけのとき真（`日` は真・`é` は `É` が仲間なので偽）。表は生成物 CaseMappingTable.hpp の
// case_fold_shared（eng/case-mapping.py が同じ 2 つの表から作る・試験が vim_folded と照合する）。
// 大小を畳むパターンの当たりをバイト列で絞り込めるかの問い（VimPattern::required_bytes）。
[[nodiscard]] bool vim_folded_ascii_comparable(char32_t value) noexcept;
} // namespace nenenib::core
