#pragma once

#include <cstdint>

namespace nenenib::core
{
// 操作の割り当ての表が使う鍵だけの閉じた一覧（ADR 0078 の決定 2）。主キーとテンキーの + - 0 は
// ui の写しが同じ値にする。鍵を足すと表示名の switch がコンパイルで落ちる（CPP-002）。
enum class OperationKey : std::uint8_t
{
    o,
    s,
    t,
    w,
    p,
    d,
    z,
    y,
    f1,
    f4,
    tab,
    plus,
    minus,
    zero
};
} // namespace nenenib::core
