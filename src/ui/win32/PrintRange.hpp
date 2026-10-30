#pragma once

#include <cstddef>

namespace nenenib::ui::win32
{
// 印刷のダイアログで選んだページの範囲（ADR 0177 の決定 3）。1 始まりの閉区間 [first, last]。
struct PrintRange
{
    std::size_t first;
    std::size_t last;
};
} // namespace nenenib::ui::win32
