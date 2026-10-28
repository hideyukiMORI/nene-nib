#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// INSERT を始めた位置（Vim の Insstart_orig・#1040）。INSERT の `Ctrl-w` `Ctrl-u` は、キャレットが
// 同じ行のこの桁より右にあるときだけここで止まる。桁は Vim と同じく行頭からのバイト数で、同じ行の
// 手前を消しても動かない。動くのはこの行を前の行とつないだときだけ（つないだ点へ・oracle で実測）。
// 矢印や Home で動いても始めの位置は変えない（`.` の記録を取り直す境界とは別・実測）。
struct VimInsertStart
{
    LineNumber line;
    std::size_t byte_column;
};
} // namespace nenenib::core
