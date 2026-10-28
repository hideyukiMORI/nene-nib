#pragma once

#include <cstdint>

namespace nenenib::core
{
// 空の文字単位の範囲を `c` がレジスタへ書くかの印（ADR 0050 の決定 3 の追補・#960）。
// writes_empty は inclusive な行末への移動（`$` `g_`・`C` も）と語のテキストオブジェクト
// （`iw` `iW` `aw` `aW`・#979）が空行で作る範囲で、Vim の op_change は消す本文が無くても空の
// 本文を yank する。ほかの空の範囲（`c0` の行頭・`ci(` の `()` など・
// exclusive で op_delete の oap->empty が先に返る）と `d` は keeps_registers のまま書かない
// （Vim 9.1 で実測）。読むのは register_after の 1 か所だけ。
enum class VimEmptyChange : std::uint8_t
{
    keeps_registers,
    writes_empty
};
} // namespace nenenib::core
