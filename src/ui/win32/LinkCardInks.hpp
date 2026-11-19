#pragma once

#include <d2d1.h>

namespace nenenib::ui::win32
{
// リンクの札の色（施主決定 D106・#1869 の段 L2）。トークンだけ（ui/win32 に色のリテラルを書かない・
// ADR 0008 の決定 8）: ground は panel、edge は panel_border、ink は text、muted は構文の comment
// （開き方の 1 行・案内・宛先のホストの外）、title は題（refused は構文の error・ほかは text）、
// accent は右のボタンの面（構文の function）、on_accent はその上の文字（本文の地）。
struct LinkCardInks
{
    D2D1_COLOR_F ground;
    D2D1_COLOR_F edge;
    D2D1_COLOR_F ink;
    D2D1_COLOR_F muted;
    D2D1_COLOR_F title;
    D2D1_COLOR_F accent;
    D2D1_COLOR_F on_accent;
};
} // namespace nenenib::ui::win32
