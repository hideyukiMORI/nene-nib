#pragma once

#include <cstdint>

namespace nenenib::core
{
// 原子 1 つに付く量の閉じた形（ADR 0184 の決定 2）。parse は `\+` `\=` `\?` `\{n,m}` `\{-n,m}` を
// 原子の写しに開くので、開いた後の原子が持つ量は（最小・最大・貪欲か）の 5 つの組だけになる:
// once は {1, 1}、star は `*`（{0, ∞, 貪欲}）、lazy_star は `\{-}`（{0, ∞, 最短}）、optional は
// `\=`（{0, 1, 貪欲}）、lazy_optional は `\{-0,1}`（{0, 1, 最短}）。`a\{2,4}` は `a a a\= a\=`、
// `a\+` は `a a*`、`a\{-1,}` は `a a\{-}`（Vim 9.1 の nfa_regpiece と同じ写し）。
enum class VimPatternRepeat : std::uint8_t
{
    once,
    star,
    lazy_star,
    optional,
    lazy_optional
};
} // namespace nenenib::core
