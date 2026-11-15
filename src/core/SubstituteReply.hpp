#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:s///c` の問いへの答え（ADR 0168 の決定 6・Vim の do_sub の typed）。ほかの鍵は答えにならず
// 問いを続ける（Vim は `^E` `^Y` で窓を送るが、段 2a では無視する・限界）。
enum class SubstituteReply : std::uint8_t
{
    // `y`: いまの当たりを置き、次の当たりを問う。
    yes,
    // `n`: いまの当たりを飛ばし、次の当たりを問う。
    no,
    // `a`: いまの当たりから後ろを全部置き、問いを終える。
    all,
    // `q` と Esc: いまの当たりを置かずに問いを終える。
    quit,
    // `l`: いまの当たりを置いて問いを終える。
    last
};
} // namespace nenenib::core
