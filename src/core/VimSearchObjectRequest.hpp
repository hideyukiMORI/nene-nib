#pragma once

#include "Selection.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchObjectFrom.hpp"

#include <cstddef>

namespace nenenib::core
{
// `gn` `gN` の 1 回の問い（Issue #1617）。selection は鍵の前の選択（NORMAL とオペレータ待ちでは
// anchor と caret が同じ）、from はそれが VISUAL の選択か、direction は `gn` が前向き・`gN` が
// 後ろ向き、count は回数（1 以上）、wrapscan は 2 回目の探索が本文の端で折り返すか（Vim の
// 'wrapscan'・1 回目はいつも折り返さない）。どの値も単独で妥当な公開 aggregate。
struct VimSearchObjectRequest
{
    Selection selection;
    VimSearchObjectFrom from;
    VimSearchDirection direction;
    std::size_t count;
    bool wrapscan;
};
} // namespace nenenib::core
