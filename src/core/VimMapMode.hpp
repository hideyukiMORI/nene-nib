#pragma once

#include <cstdint>

namespace nenenib::core
{
// `map` の表を引くときのモード（ADR 0176 の決定 5）。VimMode と違い OPERATOR-PENDING を
// 1 値に持ち、VISUAL は文字・行・矩形を 1 つに畳む。値が増えたら写し先の `switch` が落ちる。
enum class VimMapMode : std::uint8_t
{
    normal,
    insert,
    visual,
    operator_pending
};
} // namespace nenenib::core
