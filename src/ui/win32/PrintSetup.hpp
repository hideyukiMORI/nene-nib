#pragma once

#include "PrintPaper.hpp"
#include "PrintRange.hpp"

#include <vector>

namespace nenenib::ui::win32
{
// 印刷のダイアログが決めた紙と範囲（ADR 0177 の決定 3）。ranges が空なら全ページ。
struct PrintSetup
{
    PrintPaper paper;
    std::vector<PrintRange> ranges;
};
} // namespace nenenib::ui::win32
