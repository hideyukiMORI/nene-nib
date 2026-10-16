#pragma once

#include "TextBuffer.hpp"

namespace nenenib::application
{
// 開いたときに格子表示を聞くか（ADR 0140 の決定 2）。改行の索引から各行の長さ（バイト）を出し、
// giant_line_threshold をバイトで超える行だけ、コードポイントの数で確かめる。O(行数) と、
// しきい値をバイトで超える行の読み。ファイルにも文字組みにも触れない。
[[nodiscard]] bool asks_grid(const core::TextBuffer &text);
} // namespace nenenib::application
