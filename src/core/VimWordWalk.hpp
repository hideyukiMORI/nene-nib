#pragma once

#include "VimWordClass.hpp"

#include <cstddef>

namespace nenenib::core
{
// 語の移動の 1 回の問い（Issue #222）。count は歩く語の数（1 以上）、kind は語の切り方
// （w b e ge は word・W B E gE は big_word）。歩き方は kind によらず 1 本（ARC-001）。
// どの値も単独で妥当な公開 aggregate。
struct VimWordWalk
{
    std::size_t count;
    VimWordClass kind;
};
} // namespace nenenib::core
