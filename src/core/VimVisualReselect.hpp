#pragma once

#include "Offset.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimVisualExtent.hpp"

namespace nenenib::core
{
// `.` が VISUAL の変更を繰り返すときの選び直し（ADR 0033 の決定 4・5）。純関数 1 本で、
// anchor はキャレットのまま、記録した大きさぶんだけ端を伸ばした選択を返す（ARC-001）。
// 行が足りなければ最終行まで、桁が足りなければ行の内容の終わり（Vim が NUL を置く桁）まで。
// 最終行の絶対桁がキャレットより左に来る場合は後ろ向きの選択になる（Vim も同じ・実測）。
[[nodiscard]] Selection vim_visual_reselect(const TextBuffer &text, Offset caret,
                                            const VimVisualExtent &extent);
} // namespace nenenib::core
