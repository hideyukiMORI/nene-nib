#pragma once

namespace nenenib::application
{
// アクティブな文書を印刷する（ADR 0177 の決定 1）。F1 の一覧の「印刷」と Vim の `:hardcopy` が
// 同じこの 1 意図を送る。本文と字体は controller が状態から写すので中身は無い。使う人の操作なので
// keeps_tab_walk にも begin_intent の表にも書かない。
struct PrintDocument
{
};
} // namespace nenenib::application
