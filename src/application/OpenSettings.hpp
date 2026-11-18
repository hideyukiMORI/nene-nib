#pragma once

namespace nenenib::application
{
// 設定の面を開く（ADR 0170 の決定 5・2026-11-18 の追補 2）。開いたときの設定を写しに取る。使う人の
// 操作なので keeps_tab_walk にも begin_intent の表にも書かない（合図ではない）。
struct OpenSettings
{
};
} // namespace nenenib::application
