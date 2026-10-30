#pragma once

#include "ColoredRun.hpp"
#include "LineNumber.hpp"

#include <cstddef>
#include <dwrite.h>
#include <string>
#include <vector>

namespace nenenib::ui::win32
{
// 印刷の 1 行（ADR 0177 の決定 4・5）。display は core::display_line の描画の文字列（`^M`
// `<200b>` の置き換え済み）、colors は構文の色の区間、replaced は置き換えた文字の範囲（どちらも
// display の UTF-16 の範囲で前から順）。first_row は紙の幅で折り返した画面行の通し番号での始まり、
// rows はこの行の画面行の数。
struct PrintLine
{
    core::LineNumber number;
    std::string display;
    std::vector<ColoredRun> colors;
    std::vector<DWRITE_TEXT_RANGE> replaced;
    std::size_t first_row;
    std::size_t rows;
};
} // namespace nenenib::ui::win32
