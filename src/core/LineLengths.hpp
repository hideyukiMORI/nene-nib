#pragma once

#include <algorithm>
#include <cstddef>

namespace nenenib::core
{
// 本文の 1 区間の行の長さの要約（Issue #553）。長さはバイトで、'\n' そのものは数えない。
// closed は区間に '\n' が 1 つ以上あるか。closed なら head は最初の '\n' の前のバイト数、
// longest は区間の中で始まり区間の中の '\n' で終わる行の最長、tail は最後の '\n' の後のバイト数。
// closed でなければ head と tail はどちらも区間のバイト数で、longest は 0。
// 区間を並べた要約は joined で畳み、区間の境目をまたぐ行は前の tail と後の head を足して数える。
// 3 つの max は 2 引数を重ねる（initializer_list の max は STL の __std_max_8u を core
// の外へ出す）。
struct LineLengths
{
    std::size_t head = 0;
    std::size_t longest = 0;
    std::size_t tail = 0;
    bool closed = false;
};

// 隣り合う 2 区間 left・right（この順）の要約。空の区間の要約 LineLengths{} が単位元。
[[nodiscard]] constexpr LineLengths joined(const LineLengths &left,
                                           const LineLengths &right) noexcept
{
    if (!left.closed)
    {
        return LineLengths{left.head + right.head, right.longest,
                           right.closed ? right.tail : left.tail + right.tail, right.closed};
    }
    if (!right.closed)
    {
        return LineLengths{left.head, left.longest, left.tail + right.tail, true};
    }
    return LineLengths{left.head,
                       std::max(std::max(left.longest, right.longest), left.tail + right.head),
                       right.tail, true};
}

// 要約が本文全体のものとして、最長の行のバイト数（最初の行と最後の行も数える）。
[[nodiscard]] constexpr std::size_t longest_of(const LineLengths &lengths) noexcept
{
    return std::max(std::max(lengths.head, lengths.longest), lengths.tail);
}
} // namespace nenenib::core
