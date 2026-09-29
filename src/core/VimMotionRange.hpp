#pragma once

#include "OffsetRange.hpp"
#include "VimNumberedRule.hpp"
#include "VimRegisterKind.hpp"

namespace nenenib::core
{
// オペレータが覆う本文の範囲（ADR 0015 の決定 2）。「範囲を決める」と「効果にする」を分ける
// ための値で、d c y の 3 つが同じ 1 本の範囲の規則を使う。
// 文字単位は半開区間そのまま、行単位は最初の行の先頭から最後の行の内容の終わりまで
// （改行は入れない）。改行をどう足し引きするかはオペレータごとに違うので、範囲は持たない。
// numbered は削除が `"1` へ行くかの印（ADR 0050 の決定 4）。立てるのは検索の移動だけで、ほかは
// 既定の by_extent（レジスタの値で決まる）。
struct VimMotionRange
{
    OffsetRange range;
    VimRegisterKind kind;
    VimNumberedRule numbered = VimNumberedRule::by_extent;
};
} // namespace nenenib::core
