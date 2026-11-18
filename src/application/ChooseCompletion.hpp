#pragma once

namespace nenenib::application
{
// 補完の面の選んでいる候補を確定する（通常モードの Enter と Tab・ADR 0190 の決定 5）。本文に
// 入れるのは 1 編集（undo の 1 単位）で、面を閉じる。面が見えていなければ何もしない。使う人の操作
// なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ChooseCompletion
{
};
} // namespace nenenib::application
