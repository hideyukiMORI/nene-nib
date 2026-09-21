#pragma once

#include <cstdint>

namespace nenenib::core
{
// 語の切れ方の閉じた一覧（ADR 0012 の決定 5 / ADR 0031 の決定 2）。word は文字の種類の表で切り、
// big_word（Vim の WORD）は空白だけで切る。表は 1 つで、折り畳み方だけが違う（ARC-001）。
enum class VimWordClass : std::uint8_t
{
    word,
    big_word
};
} // namespace nenenib::core
