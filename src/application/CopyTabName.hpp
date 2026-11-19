#pragma once

#include "TabNamePart.hpp"

namespace nenenib::application
{
// アクティブなタブのパスかファイル名をクリップボードに写す（F1 の一覧の「パスをコピー」「ファイル名
// をコピー」・ADR 0078 の追補 #1790）。タブのメニューから選んだときは controller
// が同じ道を右クリック したタブに当てる。無題のタブは写さず 1 行知らせる。書けなければ 1
// 行知らせる（ADR 0134）。使う人の 操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct CopyTabName
{
    TabNamePart part;
};
} // namespace nenenib::application
