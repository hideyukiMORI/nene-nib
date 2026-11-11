#pragma once

#include "LayoutRect.hpp"

namespace nenenib::core
{
// 面の 1 行の部品の矩形（物理画素・#1390・採用案 docs/design/2026-11-10-explorer-look.md）。surface
// は行の丸い面（行の左右 inset の内側）、chevron は山形の升、mark はフォルダかファイルの印の升、
// name は名前の欄、dot は未保存の丸（未保存でなければ幅 0）。どれも explorer_row_parts の 1 本が
// 決め、描画と試験が同じ値を読む。どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct ExplorerRowParts
{
    LayoutRect surface;
    LayoutRect chevron;
    LayoutRect mark;
    LayoutRect name;
    LayoutRect dot;
};
} // namespace nenenib::core
