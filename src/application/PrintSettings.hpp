#pragma once

#include "PrintPaper.hpp"
#include "PrintRange.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace nenenib::application
{
// 印刷のダイアログで使う人が選んだ設定（ADR 0177 の決定 10）。ダイアログは ui が意図を送る前に
// 出し、controller と port はこの値を受けて描くだけ（port の中で modal loop を回さない）。printer
// はプリンタの名前（UTF-8）、paper は紙、ranges は 1 始まりのページの範囲（空なら全ページ）、
// ticket はダイアログの DEVMODE を写した job の print ticket の XML のバイト列（写せなければ空で
// プリンタの既定）。
struct PrintSettings
{
    std::string printer;
    PrintPaper paper;
    std::vector<PrintRange> ranges;
    std::vector<std::byte> ticket;
};
} // namespace nenenib::application
