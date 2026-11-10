#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// 回数つきの H と L の行（Vim 9.1 の normal.c の nv_scroll）。H は窓の先頭から回数 - 1 行下で、
// 窓の外へ出てもよく、本文の最終行で止まる。L は全体が見える最後の行（w_botline - 1）から
// 回数 - 1 行上で、届かなければ 1 行目。どちらも窓の大きさで切らない。オペレータの無いときは
// 呼び出し元がこの後で vim_cursor_corrected に通す（Vim はオペレータの待ちのときだけ
// cursor_correct を飛ばすので、`4y3H` は `y12H` と同じ行まで届く・#1293・ADR 0019 の追補）。
[[nodiscard]] LineNumber vim_screen_top_line(LineNumber first, std::size_t count,
                                             std::size_t line_count) noexcept;
[[nodiscard]] LineNumber vim_screen_bottom_line(LineNumber last, std::size_t count) noexcept;
} // namespace nenenib::core
