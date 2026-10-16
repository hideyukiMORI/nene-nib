#pragma once

#include "TextBuffer.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::application
{
// 開いたときに格子表示を聞くか（ADR 0140 の決定 2）。最長の行のバイト数が giant_line_threshold
// 以下なら O(塊) で聞かない（Issue #553）。超えるときだけ改行の索引から各行の長さ（バイト）を出し、
// しきい値をバイトで超える行だけ、コードポイントの数で確かめる。ファイルにも文字組みにも触れない。
[[nodiscard]] bool asks_grid(const core::TextBuffer &text);
// asks_grid と同じ答えに、答えるために改行の索引から長さを引いた行の数を添える（契約が「巨大な行の
// 無い文書では行を引かない」を確かめる読む口・Issue #553）。asks_grid はこの答えの first
// だけを返す。
[[nodiscard]] std::pair<bool, std::size_t> asks_grid_read(const core::TextBuffer &text);
} // namespace nenenib::application
