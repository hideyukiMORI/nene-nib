#pragma once

#include "Selection.hpp"
#include "VimMotionRange.hpp"

namespace nenenib::core
{
// テキストオブジェクトが決めた範囲と、VISUAL がそこに置く選択（ADR 0031 の決定 4 と
// Issue #99 の補足）。オペレータの後ろでは range だけ、VISUAL では selection だけを使う。
// 両端の置き方は種類ごとに違う（括弧は必ず前向き・引用符は今の選択の向きのまま・語は
// 後ろ向きの選択では anchor を動かさない。どれも Vim 9.1 で実測）ので、1 本の範囲関数が
// そこまで決める（ARC-001）。位置も範囲も単独で妥当なので公開 aggregate（ADR 0007）。
struct VimTextObjectSpan
{
    VimMotionRange range;
    Selection selection;
};
} // namespace nenenib::core
