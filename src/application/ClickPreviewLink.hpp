#pragma once

#include "PreviewPoint.hpp"

namespace nenenib::application
{
// プレビューの文字の上の左クリック（施主決定 D106・#1869 の段 L2）。pressed は押した点、released
// は離した点の下の文字（ui が最後に描いた layout の当たりで求めた要素の番号と text のバイト位置・
// 文字の上でなければ ui は送らない）。controller は 2 つが同じリンクに当たるときだけ札を開き
// （ADR 0056 の決定 9 の形）、ほかは開いている札を閉じるだけ。使う人の操作なので keeps_tab_walk
// にも begin_intent の表にも書かない。
struct ClickPreviewLink
{
    core::PreviewPoint pressed;
    core::PreviewPoint released;
};
} // namespace nenenib::application
