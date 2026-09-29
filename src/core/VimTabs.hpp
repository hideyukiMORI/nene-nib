#pragma once

#include <cstddef>

namespace nenenib::core
{
// engine が借用するタブの今の値（ADR 0057 の決定 2）。active は帯の上の今の位置（0 始まり）、
// count は本数。controller が鍵ごとに今の値で作り、engine は `gt` `gT` の行き先と失敗を
// tab_destination で決めるのに読むだけ。状態の欄には置かない。
struct VimTabs
{
    std::size_t active;
    std::size_t count;
};
} // namespace nenenib::core
