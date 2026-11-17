#pragma once

namespace nenenib::application
{
// プロジェクトの全文検索の面を開く（ADR 0167 の決定 1・操作 search_project・Ctrl+Shift+F）。入力が
// `!` の Ctrl+P の面で、開いている入力行や面は取消と同じ道で閉じてから開く。使う人の操作で、
// OpenOperationList と同じく Ctrl+Tab の歩きを確定し、1 行の知らせを消す（keeps_tab_walk と
// begin_intent の表に載せない）。合図ではない（is_signal に載せない）。
struct OpenProjectSearch
{
};
} // namespace nenenib::application
