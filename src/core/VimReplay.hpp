#pragma once

#include "VimKey.hpp"

#include <vector>

namespace nenenib::core
{
// `.` の再生（ADR 0030 の決定 6）。回数の桁を先頭に展開した鍵の列だけを運び、controller が
// 同じ accept の経路へ 1 つずつ流す（決定 7）。本文もキャレットも持たない。
struct VimReplay
{
    std::vector<VimKey> keys;
};
} // namespace nenenib::core
