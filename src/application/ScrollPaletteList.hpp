#pragma once

#include <cstdint>

namespace nenenib::application
{
// Ctrl+P の面の上のホイール（ADR 0150 の決定 2）。一覧だけを lines 行送り、選択は動かさない
// （見えなくなったら窓の端へ寄せる）。正は下（後ろの候補）へ。面が開いていなければ何もしない。
struct ScrollPaletteList
{
    std::int32_t lines;
};
} // namespace nenenib::application
