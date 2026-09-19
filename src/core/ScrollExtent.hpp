#pragma once

#include <cstdint>

namespace nenenib::core
{
// 縦スクロールで許す末尾の形。通常編集は画面を本文で埋め、Vim は最終行を先頭にして
// その下に空白を残せる（ADR 0019 の決定 4）。
enum class ScrollExtent : std::uint8_t
{
    filled_viewport,
    last_line
};
} // namespace nenenib::core
