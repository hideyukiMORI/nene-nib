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

// 色を付ける囲みの上限。行かバイトを超える囲みは 1 色のまま（字句解析もしない）。
inline constexpr std::size_t preview_code_color_line_limit = 5000;
inline constexpr std::size_t preview_code_color_byte_limit = std::size_t{512} * 1024;
// 1 つの囲みの色の区間の数の上限（隣り合う同じ役の区間を畳んだ後）。超える囲みは 1 色のまま。
// ui が区間ごとに置く SetDrawingEffect は区間の数の 2 乗で伸びるので（#1860 のレビュー M1・
// 2,500 区間で約 8 ms・5,000 区間で約 37 ms の実測）、上限いっぱいの囲みで 1 回 10 ms
// 前後に収める。
inline constexpr std::size_t preview_code_color_count_limit = 2500;

// info string の最初の語（md4c の lang・`,` より後ろは読まない）から言語を引く。ASCII
// の大文字小文字を畳んで language_of_label で引き、字句器を持たない言語（plain）と
// markdown（入れ子を読まない）は空。
[[nodiscard]] std::optional<Language> preview_code_language(std::string_view word);

// text（囲みのコードの要素の text・行は '\n' で切れる）を language の字句器で読んだ色の区間。
// 上限を超えれば空（1 色）。区間は text の中で昇順・重ならない・空の区間を含まない・隣り合う
// 同じ役の区間は 1 つに畳む。始まりの状態は lex_start_state(language, false)。
[[nodiscard]] std::optional<std::vector<PreviewCodeColor>>
preview_code_colors(Language language, std::string_view text);
} // namespace nenenib::core
