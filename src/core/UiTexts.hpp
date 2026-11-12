#pragma once

#include "DisplayLanguage.hpp"
#include "UiMessage.hpp"

#include <initializer_list>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 画面に出る文を引く口（ADR 0123 の追補 2026-11-13 の決定 4・5）。表は言語ごとの列のファイル
// （UiTextsJapanese.hpp ほか）で、ここは添字 2 つで引くだけ。返す string_view は静的な表を指す。
[[nodiscard]] std::string_view ui_text(UiMessage message, DisplayLanguage language) noexcept;

// 置き場 {0}〜{9} を values の同じ番号の値で埋めた文（決定 5）。置き場より値が少なければ足りない
// 置き場をそのまま残し、多ければ余りを捨てる。同じ置き場が 2 回あれば 2 回とも埋める。置き場の形
// でない `{` はそのまま。語順は言語ごとの文が決めるので、呼び出し元で断片を連結しない。
[[nodiscard]] std::string ui_text_filled(UiMessage message, DisplayLanguage language,
                                         std::initializer_list<std::string_view> values);

// ui_text_filled の埋め方の本体（表を引かない純関数・試験が表に無い形の文で埋め方を確かめる口）。
[[nodiscard]] std::string placeholders_filled(std::string_view text,
                                              std::initializer_list<std::string_view> values);
} // namespace nenenib::core
