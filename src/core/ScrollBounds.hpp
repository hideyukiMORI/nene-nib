#pragma once

#include "LineNumber.hpp"
#include "ScrollExtent.hpp"
#include "ScrollFollow.hpp"

#include <cstddef>

namespace nenenib::core
{
// 縦スクロールの上限とキャレット追従の純関数（ADR 0009 の決定 6）。
// 量そのものは application の ScrollState が所有し、ここは「どこまで動けるか」だけを決める。

// 先頭行を本文の範囲に収める。末尾の許し方は extent が決める。
[[nodiscard]] LineNumber first_visible_within(LineNumber requested, std::size_t total_lines,
                                              std::size_t visible_lines,
                                              ScrollExtent extent) noexcept;

// キャレットの行が見えるところまで先頭行を寄せる。見えていれば動かさない。
[[nodiscard]] LineNumber first_visible_for_caret(LineNumber first_visible, LineNumber caret_line,
                                                 std::size_t visible_lines,
                                                 ScrollFollow follow) noexcept;
} // namespace nenenib::core
