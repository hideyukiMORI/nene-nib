#pragma once

#include "IndentGuides.hpp"

namespace nenenib::ui::win32
{
// 1 行のインデントガイドを描く位置（ADR 0193 の決定 9）。left は行の文字組みの原点の x（本文の欄の
// 左端 ＋ 横の送り・line_area の left）、top と height は最初の画面行（折り返した行の 2
// 行目以降には 引かない）、space は本文の書式の空白の幅（BodyTextLayouts::space_width）、thin と
// thick は線の 太さ（1 DIP と 2 DIP の物理画素）。公開 aggregate（CPP-003）。
struct IndentGuideRow
{
    application::IndentGuides guides;
    float left;
    float top;
    float height;
    float space;
    float thin;
    float thick;
};
} // namespace nenenib::ui::win32
