#pragma once

#include "VimRecordedKeys.hpp"

#include <cstddef>

namespace nenenib::core
{
// 回数付きの INSERT（i a I A o O）のあいだだけ持つ、残り回数と打った鍵の列（ADR 0028・#1093）。
// Esc で列を残りの回数だけ流し直す（Vim の start_redo_ins が redo の打鍵を流すのと同じ）。列に
// 載るのは本文を変えた鍵だけで、文字・改行・消す鍵（Backspace・Ctrl-w・Ctrl-u）と、Ctrl-r が
// 入れた本文の文字。o / O は改行の鍵 1 つから始まる。本文は所有しない。列は `.` の記録と同じ
// VimRecordedKeys で持ち（1 打鍵ごとに状態と一緒に写しても塊を共有する・ADR 0089）、Esc で 1 回
// だけ平坦にする。
struct VimInsertRepeat
{
    std::size_t remaining;
    VimRecordedKeys keys;
};
} // namespace nenenib::core
