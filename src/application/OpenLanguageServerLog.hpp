#pragma once

namespace nenenib::application
{
// 言語サーバーの記録を無題のタブに写して開く（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。F1 の
// 一覧の操作「言語サーバーの記録を開く」が送り、Ex の `:LspLog` も同じ道を通る。中身は無く、記録が
// 空なら controller が 1 行知らせて開かない。使う人の操作なので keeps_tab_walk にも begin_intent の
// 表にも書かない。
struct OpenLanguageServerLog
{
};
} // namespace nenenib::application
