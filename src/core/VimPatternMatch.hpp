#pragma once

#include <cstddef>

namespace nenenib::core
{
// 1 行の中の一致（半開区間・行の先頭からのバイト位置）。長さ 0 の一致では begin == end。
// 本文の中の位置ではないので Offset ではない（単位を型で持つ・CPP-001）。
struct VimPatternMatch
{
    std::size_t begin;
    std::size_t end;
};
} // namespace nenenib::core
