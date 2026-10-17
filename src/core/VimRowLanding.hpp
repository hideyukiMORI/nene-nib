#pragma once

#include "VimMotionLanding.hpp"
#include "VimWantedColumn.hpp"

namespace nenenib::core
{
// `gj` `gk` の着地（ADR 0144 の決定 3・4）。landing は行き先の画面行の中で欲しい列に達する文字
// （モードの寄せの前）と、回数の途中で本文の先頭 / 末尾の画面行に当たったときの失敗の印。wanted は
// 着いた後の欲しい列（行頭からの仮想桁・$ の後は行末のまま）。engine が鍵を流す前に 1 打鍵の間だけ
// 解き、同じ motion / range / selection の経路が読む。
struct VimRowLanding
{
    VimMotionLanding landing;
    VimWantedColumn wanted;
};
} // namespace nenenib::core
