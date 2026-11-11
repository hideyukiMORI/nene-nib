#pragma once

#include "LayoutRect.hpp"

namespace nenenib::core
{
// 起動画面の選べる行 1 つの配置（物理画素・ADR 0172 の決定 5）。line は列の全幅の行（選んだ行の
// current_line の地とクリックの当たり）、mark は選んだ印 ▸ の升、label は項目の欄（名前と場所・
// 「はじめる」の名前と補足・右端の鍵の枠を、描画が測った幅で中に置く・追補 2026-11-11 の 17）。
// 公開 aggregate（CPP-003）。
struct StartRowLayout
{
    LayoutRect line;
    LayoutRect mark;
    LayoutRect label;
};
} // namespace nenenib::core
