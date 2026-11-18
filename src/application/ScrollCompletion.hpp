#pragma once

#include <cstdint>

namespace nenenib::application
{
// 補完の面の上のホイール（ADR 0190 の決定 5・#1844 のレビュー R1）。面の窓だけを lines 行送り
// （正は下・後ろの候補へ）、選んでいる行は見えなくなったら窓の端へ寄せる。面が見えていなければ
// 何もしない。本文もキャレットも動かさない。使う人の操作なので keeps_tab_walk にも begin_intent の
// 表にも書かない。
struct ScrollCompletion
{
    std::int32_t lines;
};
} // namespace nenenib::application
