#pragma once

#include <cstdint>

namespace nenenib::core
{
// 隣のタブへ移る向き（ADR 0056 の決定 3）。帯の位置の順で、端は折り返す（使った順にはしない）。
// 値が増えたら写し先の足りない `switch` がコンパイルで落ちる（CPP-002）。
enum class TabStep : std::uint8_t
{
    next,
    previous
};
} // namespace nenenib::core
