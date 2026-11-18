#pragma once

namespace nenenib::application
{
// 補完の面を手で開く（操作「補完」・通常モードの Ctrl+Space と F1 の一覧・ADR 0190 の決定 5）。
// 候補は LSP の候補（動いている文書だけ・先）と本文の語（後）。どちらも無ければ開かずに 1 行知らせる。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない（歩きを終え、告知を消す）。
struct OpenCompletion
{
};
} // namespace nenenib::application
