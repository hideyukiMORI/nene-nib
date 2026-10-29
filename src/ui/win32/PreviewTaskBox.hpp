#pragma once

#include "LayoutRect.hpp"

namespace nenenib::ui::win32
{
// タスクリストの項目のチェックボックスの矩形（ADR 0155 の追補 23・24）。座標は枠の左上からの
// 物理画素（送りを除く）。square は描く正方形（本文の字の大きさ・1 行目の縦の中央）、target は
// 押す範囲（1 行の高さの正方形に左右 2 DIP の余裕・正方形と中心が同じ）。PreviewBlockTops が
// 要素ごとに 1 つ保持し、PreviewPainter（描く）と PreviewPane（hit test）が同じ値を読む。
// どちらも単独で妥当な矩形なので公開 aggregate（CPP-003）。
struct PreviewTaskBox
{
    core::LayoutRect square;
    core::LayoutRect target;
};
} // namespace nenenib::ui::win32
