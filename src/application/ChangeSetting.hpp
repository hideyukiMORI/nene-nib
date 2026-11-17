#pragma once

namespace nenenib::application
{
// 焦点の行の値を変える（追補 2）。スイッチの行は forward に依らず切り替え（Space）、値の行は
// forward で次・前へ送る（左右の鍵）。切れない機能の行は何もしない。変えた設定はその場で状態に
// 置き（frame が作り直される）、保存は閉じるとき。使う人の操作なので keeps_tab_walk にも
// begin_intent の表にも書かない。
struct ChangeSetting
{
    bool forward;
};
} // namespace nenenib::application
