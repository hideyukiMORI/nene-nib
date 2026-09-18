#pragma once

#include <cstdint>

namespace nenenib::core
{
// e が語の末尾にいたときの振る舞い（Vim の end_word の stop 引数）。
// 素の e は次の語の末尾へ進み、cw の特例は動かない（cw が語の最後の 1 文字だけを変える理由）。
// 回数があるときに効くのは最初の 1 回だけで、2 回目からは進む側になる。
enum class VimWordEndStop : std::uint8_t
{
    enter_the_next_word,
    stay_in_this_word
};
} // namespace nenenib::core
