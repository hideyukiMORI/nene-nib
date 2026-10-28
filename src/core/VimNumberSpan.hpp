#pragma once

#include "VimNumberForm.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 見つけた数（ADR 0015 の決定 14）。`begin` `end` は置き換える窓の中のバイト範囲で、10 進の
// 負の数なら `-` を含む（16 進と 2 進の前の `-` は数に入らない・Vim の do_addsub）。
// `magnitude` は符号を除いた値（64 bit で溢れたら最大値に貼り付き `overflow` が立つ）。
// `upper_digits` は範囲の最後の英字が大文字か（16 進の桁の大小）、`upper_prefix` は `0X` `0B`、
// `zero_led` は最初の桁が `0` か（桁数を保って 0 で埋める）。
struct VimNumberSpan
{
    std::size_t begin;
    std::size_t end;
    std::uint64_t magnitude;
    VimNumberForm form;
    bool negative;
    bool overflow;
    bool upper_digits;
    bool upper_prefix;
    bool zero_led;
};
} // namespace nenenib::core
