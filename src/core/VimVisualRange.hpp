#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimMode.hpp"
#include "VimMotionRange.hpp"

namespace nenenib::core
{
// VISUAL の選択が覆う本文の範囲（ADR 0018 の決定 4）。描く選択も Ctrl+C / Ctrl+X も
// `d x y c` も、この 1 本が決めた範囲を使う（ARC-001）。純関数。
//
// `visual` は小さいほうの位置から、大きいほうの位置の次の code point まで（Vim の inclusive）。
// 大きいほうが行の内容の終わり（Vim が NUL を置く桁）に載っていれば改行を含む
// ＝ `v$d` が次の行と繋ぎ、空行の `v` が改行 1 つを選ぶ（Issue #53 で実測）。
// `visual_line` は小さいほうの行の行頭から大きいほうの行の内容の終わりまで（行単位）。
// NORMAL / INSERT で呼ぶと caret の所の空の範囲（選択はそこに無い）。
[[nodiscard]] VimMotionRange vim_visual_range(const TextBuffer &text, const Selection &selection,
                                              VimMode mode);
} // namespace nenenib::core
