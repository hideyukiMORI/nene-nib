#pragma once

#include "TextPosition.hpp"
#include "VimSearchDirection.hpp"

#include <cstddef>

namespace nenenib::core
{
// 次の一致を求める 1 回の問い（ADR 0041 の決定 1）。from は探し始める位置、direction はこの 1 回に
// 効く向き、count は続けて探す回数（1 以上）。確定の検索の鍵と incsearch の preview
// が同じ形で渡す。 どの値も単独で妥当な公開 aggregate。wrapscan は本文の端で折り返すか（Vim の
// 'wrapscan'・既定は真・`:set nows` が偽にする・#1561）。偽なら端を越える当たりは
// hit_bottom_without_match / hit_top_without_match。
struct VimMatchRequest
{
    TextPosition from;
    VimSearchDirection direction;
    std::size_t count;
    bool wrapscan = true;
};
} // namespace nenenib::core
