#pragma once

#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimState.hpp"

namespace nenenib::core
{
// 1 つの鍵の結果。次の状態と、application が本文へ写す効果 1 つ（ADR 0012 の決定 2）。
struct VimStep
{
    VimState next;
    VimEffect effect;
};

// Vim エンジンの唯一の入口（ARC-001）。純関数で、時刻・OS・スレッドを持たない（ARC-007）。
// 選択をまるごと受け取る（ADR 0018 の決定 2）。NORMAL / INSERT は anchor == caret で caret しか
// 読まず、VISUAL だけが anchor を範囲の片端として読む。
[[nodiscard]] VimStep vim_step(const VimState &state, const TextBuffer &text,
                               const Selection &selection, VimKey key);
} // namespace nenenib::core
