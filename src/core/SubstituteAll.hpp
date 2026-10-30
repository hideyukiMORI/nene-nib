#pragma once

#include "Edit.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 本文の当たりをすべて replacement で置き換えた 1 回の Edit（ADR 0174 の決定 7・ADR 0168 の
// `:%s` の全件と同じ 1 本）。純関数で、本文・状態を持たない。当たりは行ごとに vim_line_matches の
// 1 本で前から数え（重なる当たりは飛ばす・Vim の `:s///g` と同じ）、長さ 0 の当たりは置き換えない
// （通常モードの語は literal なので長さ 0 にならない）。Edit は最初の当たりの始まりから最後の
// 当たりの終わりまでの 1 つの範囲で、当たりの間の本文はそのまま写す。replacement は literal
// （`\n` `&` `\1` を解釈しない・決定 7 の限界）。restore は最初の当たりの始まりで、本文へ当てる
// 側（controller の replace）がそのときのキャレットで書き直す。当たりが無ければ値なし。
[[nodiscard]] std::optional<Edit> substituted_all(const TextBuffer &text, const VimPattern &pattern,
                                                  std::string_view replacement);
} // namespace nenenib::core
