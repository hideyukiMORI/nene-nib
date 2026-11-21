#pragma once

#include "Language.hpp"
#include "PreviewCodeColor.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 囲みのコードを構文の色で描くための純関数（#1858・ADR 0151 / 0156）。言語の表は LanguageTable の
// 1 つ（language_of_label）、字句器は本文と同じ lex_line で、行をまたぐ状態も本文と同じく行ごとに
// 引き継ぐ。parse_markdown が要素を閉じるときに 1 回だけ呼び、ui は答えを描くだけ。

// 色を付ける囲み全体の上限。行かバイトを超える囲みは全部の切れが 1 色のまま（字句解析もしない）。
inline constexpr std::size_t preview_code_color_line_limit = 5000;
inline constexpr std::size_t preview_code_color_byte_limit = std::size_t{512} * 1024;
// 1 つの切れの色の区間の数の上限（隣り合う同じ役の区間を畳んだ後）。超える切れは 1 色のまま。
// ui が区間ごとに置く SetDrawingEffect は 1 つの layout の区間の数の 2 乗で伸びるので（#1860 の
// レビュー M1・2,500 区間で約 8 ms・5,000 区間で約 37 ms の実測）、上限いっぱいの切れで 1 回
// 10 ms 前後に収める。
inline constexpr std::size_t preview_code_color_count_limit = 2500;
// 囲みのコードの切れ（要素）の行の数（ADR 0151 の追補 2026-11-19・#1863。2026-11-20 に 128 から
// 64 へ・#1913・新しい切れが枠に入る frame の文字組みの費用を半分にする）。これを超える囲みは
// この行ごとの続く要素の列で出し、layout と色の区間は切れごとに持つ。
inline constexpr std::size_t preview_code_piece_lines = 64;

// info string の最初の語（md4c の lang・`,` より後ろは読まない）から言語を引く。ASCII
// の大文字小文字を畳んで language_of_label で引き、字句器を持たない言語（plain）と
// markdown（入れ子を読まない）は空。
[[nodiscard]] std::optional<Language> preview_code_language(std::string_view word);

// text（囲みのコードの要素の text・行は '\n' で切れる）の切れの始まりのバイト位置の列。最初は 0
// で、preview_code_piece_lines 行ごとに次の行頭。最後の改行の後ろに文字が無ければ切れを足さない
// （64 行ちょうどの囲みは切れ 1 つ）。
[[nodiscard]] std::vector<std::size_t> preview_code_piece_starts(std::string_view text);

// text を language の字句器で囲み全体で 1 回読んだ、切れ（starts は preview_code_piece_starts(text)
// の答え）ごとの色の区間。区間は切れの text
// の中のバイト位置で、昇順・重ならない・空の区間を含まない・ 隣り合う同じ役の区間は 1
// つに畳む。行をまたぐ状態は切れの境を越えて引き継ぎ、始まりの状態は lex_start_state(language,
// false)。囲み全体が行かバイトの上限を超えれば全部の切れが空（1 色・
// 字句解析もしない）。区間の数の上限を超えた切れだけ空で、その切れの区間はその時点で捨てる。
[[nodiscard]] std::vector<std::optional<std::vector<PreviewCodeColor>>>
preview_code_piece_colors(Language language, std::string_view text,
                          const std::vector<std::size_t> &starts);
} // namespace nenenib::core
