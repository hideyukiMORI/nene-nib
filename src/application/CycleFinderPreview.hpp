#pragma once

namespace nenenib::application
{
// 探して見る面のプレビュー欄の置き方を次へ（ADR 0189 の決定 3・操作 cycle_finder_preview）。面が
// 開いている間の Ctrl+Shift+P・見出しの右端の印・F1 の一覧から。順は right → below → off → right
// （今 automatic なら今描いている形の次）で、選んだ形は設定に覚える。使う人の操作で、
// OpenProjectSearch と同じく Ctrl+Tab の歩きを確定し、1 行の知らせを消す（keeps_tab_walk と
// begin_intent の表に載せない）。合図ではない（is_signal に載せない）。
struct CycleFinderPreview
{
};
} // namespace nenenib::application
