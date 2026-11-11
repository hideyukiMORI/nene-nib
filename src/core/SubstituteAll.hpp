#pragma once

#include "Edit.hpp"
#include "SubstituteFailure.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace nenenib::core
{
// 本文の当たりをすべて replacement で置き換えた 1 回の Edit（ADR 0174 の決定 7・ADR 0168 の
// `:%s` の全件と同じ 1 本）。純関数で、本文・状態を持たない。当たりは行ごとに vim_line_matches の
// 1 本で前から数え（重なる当たりは飛ばす・Vim の `:s///g` と同じ）、長さ 0 の当たりは置き換えない
// （通常モードの語は literal なので長さ 0 にならない）。Edit は最初の当たりの始まりから最後の
// 当たりの終わりまでの 1 つの範囲で、当たりの間の本文はそのまま写す。replacement は literal
// （`\n` `&` `\1` を解釈しない・決定 7 の限界）。restore は最初の当たりの始まりで、本文へ当てる
// 側（controller の replace）がそのときのキャレットで書き直す。当たりが無ければ no_match。
// 置き換えた後の文書のバイト数（本文 - 当たりの和 + 当たりの数 × replacement）が limit（製品は
// maximum_document_bytes・試験だけが小さな値を渡す）を超えるなら、当たりを数えるだけの 1 周目の
// 後、置き換えた本文を確保する前に too_large（#1376・#1341 と同じ上限）。当たりの位置の列は
// 持たず（本文の 16 倍になり得る）、置き換える 2 周目で最初の当たりの行から最後の当たりの行まで
// の行を全部照合し直す（その外の行は飛ばす）。
[[nodiscard]] std::expected<Edit, SubstituteFailure> substituted_all(const TextBuffer &text,
                                                                     const VimPattern &pattern,
                                                                     std::string_view replacement,
                                                                     std::size_t limit);
} // namespace nenenib::core
