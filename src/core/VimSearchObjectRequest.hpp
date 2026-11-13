#pragma once

#include "Selection.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchObjectFrom.hpp"

#include <cstddef>

namespace nenenib::core
{
// `gn` `gN` の 1 回の問い（Issue #1617）。selection は鍵の前の選択（NORMAL とオペレータ待ちでは
// anchor と caret が同じ）、from はそれが VISUAL の選択か、direction は `gn` が前向き・`gN` が
// 後ろ向き、count は回数（1 以上）。どの値も単独で妥当な公開 aggregate。
struct VimSearchObjectRequest
{
    Selection selection;
    VimSearchObjectFrom from;
    VimSearchDirection direction;
    std::size_t count;
};
} // namespace nenenib::core
