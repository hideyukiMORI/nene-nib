#pragma once

#include <cstdint>

namespace nenenib::core
{
// 文字単位の範囲をレジスタへ書くときの形（Issue #1364）。Vim の op_yank は「exclusive の文字単位で
// 始まりも終わりも 1 桁目、2 行以上」の範囲を行単位で yank する（register.c・「the yank is always
// linewise」）。消す本文は文字単位のまま変わらない。exclusive の言い換え（exclusive_range）が
// 通らない範囲だけがこの形のまま残り、いまは d<BS> c<BS> が前の行へ渡ったとき（Vim の nv_left の
// CA_NO_ADJ_OP_END）だけが whole_lines を立てる。読むのは register_of の 1 か所だけ。
enum class VimYankShape : std::uint8_t
{
    by_kind,
    whole_lines
};
} // namespace nenenib::core
