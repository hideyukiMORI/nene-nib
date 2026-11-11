#pragma once

#include "LayoutRect.hpp"

namespace nenenib::core
{
// 起動画面の選べる行 1 つの配置（物理画素・ADR 0172 の決定 5）。line は列の全幅の行（選んだ行の
// current_line の地とクリックの当たり）、mark は選んだ印 ▸ の升、label は項目（パスか「はじめる」の
// 名前）、side は右端の補足（「はじめる」の行だけ・ほかは幅 0）。公開 aggregate（CPP-003）。
struct StartRowLayout
{
    LayoutRect line;
    LayoutRect mark;
    LayoutRect label;
    LayoutRect side;
};
} // namespace nenenib::core
