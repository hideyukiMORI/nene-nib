#pragma once

#include "VimEditorView.hpp"
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
// 本文・選択・表示領域を 1 回だけ借用する（ADR 0019 の決定 1）。NORMAL / INSERT は
// selection の caret だけを読み、VISUAL は anchor も範囲の片端として読む。
[[nodiscard]] VimStep vim_step(const VimState &state, const VimEditorView &view, VimKey key);
} // namespace nenenib::core
