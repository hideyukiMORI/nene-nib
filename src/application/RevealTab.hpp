#pragma once

namespace nenenib::application
{
// アクティブなタブのファイルを選んだ状態でエクスプローラーにフォルダを開かせる（F1 の一覧の
// 「エクスプローラーで場所を開く」・ADR 0078 の追補 #1790）。タブのメニューから選んだときは
// controller が同じ道を右クリックしたタブに当てる（まだ読んでいないタブも読まずにパスで）。無題の
// タブは頼まず 1 行知らせる。頼むのは RevealPort で、失敗は裏で届いてから 1 行知らせる。使う人の
// 操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct RevealTab
{
};
} // namespace nenenib::application
