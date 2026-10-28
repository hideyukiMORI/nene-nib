#pragma once

#include <cstdint>

namespace nenenib::application
{
// 面に焦点があるときの鍵（ADR 0162 の決定 4）。ui が打った鍵をこの値へ写し、Vim の鍵の表は
// 通らない。j と down、k と up、l と right、h と left はそれぞれ同じ動き（鍵の写しを表のまま
// 書けるよう値は分ける）。値が増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class ExplorerKeyKind : std::uint8_t
{
    j,
    k,
    h,
    l,
    enter,
    esc,
    down,
    up,
    left,
    right
};
} // namespace nenenib::application
