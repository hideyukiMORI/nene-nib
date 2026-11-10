#pragma once

#include <cstdint>

namespace nenenib::core
{
// 置き換え（VimReplaceRange）の後にマークの行を直すか（ADR 0152 の決定 6 の追補・#1360）。
// shifted は Vim が mark_adjust を呼ぶ置き換え（J gJ の do_join・NORMAL の r<CR>・INSERT の回数の
// 流し直し）で、marks_adjusted が行のずれを写す。kept は行を 1 行ずつその場で書き換える置き換え
// （`>` `<` の op_shift・`~` `u` `U` `g~` `gu` `gU` の op_tilde・VISUAL の r の op_replace・
// Ctrl-a Ctrl-x の op_addsub）で、Vim は mark_adjust を呼ばないので、範囲の中のマークも動かさない。
enum class VimLineMarks : std::uint8_t
{
    shifted,
    kept
};
} // namespace nenenib::core
