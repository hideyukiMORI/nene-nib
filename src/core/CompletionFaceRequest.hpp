#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 補完の面の配置の入力（ADR 0169 の決定 4・ADR 0190 の決定 5）。caret はキャレットの矩形
// （クライアント座標の物理画素・renderer が描いた値）、x は面の左端にしたい位置（面を開いたときの
// キャレットの左・打っている間は動かさない）、bounds は収める矩形。rows は表示値に載っている行の数
// （1 以上）、selected はその中の選んでいる行、detailed は補足（detail）の欄を出すこと。
struct CompletionFaceRequest
{
    LayoutRect caret;
    std::int32_t x;
    LayoutRect bounds;
    std::size_t rows;
    std::size_t selected;
    bool detailed;
    std::uint32_t dpi;
};
} // namespace nenenib::core
