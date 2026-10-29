#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::core
{
// INSERT を始めた位置（Vim の Insstart_orig・#1040）。INSERT の `Ctrl-w` `Ctrl-u` は、キャレットが
// 同じ行のこの桁より右にあるときだけここで止まる。桁は Vim と同じく行頭からのバイト数で、同じ行の
// 手前を消しても動かない。動くのは、この行を前の行とつないだとき（つないだ点へ）と、矢印・Home
// などで動いた後に打った最初の鍵で、キャレットの桁が始めの桁以下だったとき（キャレットへ・
// 右なら動かない）。moved は移動の後でまだ打っていない印（Vim の arrow_used・oracle で実測）。
struct VimInsertStart
{
    LineNumber line;
    std::size_t byte_column;
    bool moved;
};
} // namespace nenenib::core
