#pragma once

#include <cstdint>

namespace nenenib::core
{
// w W の 1 周（Vim の fwd_word の 1 周）の結果（Issue #226）。fwd_word が FAIL を返すのは周の
// 始めの 1 歩で本文の最後の文字から出られないときだけで、周の途中で本文が尽きたら（行末で止まる
// オペレータの特例を含む）回数の残りを捨てて OK を返す。
enum class VimWordAdvance : std::uint8_t
{
    // 次の語の頭へ着いた。回数の残りを続ける。
    continues,
    // 周の途中で本文が尽きたか行末で止まった。そこが着地（Vim の return OK）。
    stopped,
    // 周の始めに本文の最後の文字（か終わり）にいて出られなかった（Vim の return FAIL）。
    failed_at_the_end
};
} // namespace nenenib::core
