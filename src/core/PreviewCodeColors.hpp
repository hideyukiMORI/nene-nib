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

// 色を付ける囲みの上限。どちらかを超える囲みは 1 色のまま（字句解析もしない）。
inline constexpr std::size_t preview_code_color_line_limit = 5000;
inline constexpr std::size_t preview_code_color_byte_limit = std::size_t{512} * 1024;

// info string の最初の語（md4c の lang）から言語を引く。ASCII の大文字小文字を畳んで
// language_of_label で引き、字句器を持たない言語（plain）と markdown（入れ子を読まない）は空。
[[nodiscard]] std::optional<Language> preview_code_language(std::string_view word);

// text（囲みのコードの要素の text・行は '\n' で切れる）を language の字句器で読んだ色の区間。
// 上限を超えれば空（1 色）。区間は text の中で昇順・重ならない・空の区間を含まない。
[[nodiscard]] std::optional<std::vector<PreviewCodeColor>>
preview_code_colors(Language language, std::string_view text);
} // namespace nenenib::core
