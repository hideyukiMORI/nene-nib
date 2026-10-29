#pragma once

#include <cstddef>

namespace nenenib::core
{
// 追従の寄せが見る窓の行数と上下の余白（Vim の `scrolloff`・#1041）。余白は寄せる側が画面の
// 半分で切り詰めるので、ここは設定の値のまま。
struct ViewportRows
{
    std::size_t visible_lines;
    std::size_t margin;
};
} // namespace nenenib::core
