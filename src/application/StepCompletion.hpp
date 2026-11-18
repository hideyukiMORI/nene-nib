#pragma once

#include "CompletionStep.hpp"

namespace nenenib::application
{
// 補完の面の上下と PageUp / PageDown（ADR 0190 の決定 5）。面が見えていなければ何もしない。本文も
// キャレットも動かさない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct StepCompletion
{
    CompletionStep step;
};
} // namespace nenenib::application
