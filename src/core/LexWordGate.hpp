#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 字句器の表引きの門（#859・ADR 0156 の追補）。識別子を表で二分探索する前に、当たる見込みの無い
// 語を表を引かずに落とす。門は表そのものから consteval で作り（LexScan.hpp の lex_gate_of）、
// 門のための別の表を書かない。偽陽性は許し（門を通っても表に無い語は二分探索が落とす）、偽陰性は
// 無い（表の語は全部通る・lex_in / lex_folded_in の static_assert）。
struct LexWordGate
{
    // 表の語の先頭バイトの集合（256 ビット・folded なら ASCII の小文字に畳んだバイト）。
    std::array<std::uint64_t, 4> first{};
    // 表の語の長さの最小・最大（バイト）。
    std::size_t shortest = 0;
    std::size_t longest = 0;
    // 大小を問わない表（lex_folded_in）の門か。
    bool folded = false;
};
} // namespace nenenib::core
