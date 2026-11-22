#pragma once

#include "PaneSizing.hpp"

namespace nenenib::application
{
// 鍵で操作中の枠の大きさを変える（ADR 0148 の 2026-11-22 の追補・#1941）。使う人の操作。通常モード
// の操作（F1 の一覧の「枠を広げる」「枠を狭める」「枠を等分に戻す」）が送る。Vim の `Ctrl-w` と Ex
// の `:resize` は同じ値を controller の中で同じ 1 本（resize_pane）へ渡す。
struct ResizePane
{
    core::PaneSizing sizing;
};
} // namespace nenenib::application
