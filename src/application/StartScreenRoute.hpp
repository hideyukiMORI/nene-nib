#pragma once

#include <cstdint>

namespace nenenib::application
{
// 起動画面が出ている間に届いた意図の行き先（ADR 0172 の決定 4・追補 2026-11-11 の 5）。決めるのは
// controller の入口の 1 か所（start_screen_route）で、意図の種類と鍵と選びの有無だけから引く。
// pass は今までどおり本文へ写す。next / previous は選びを動かす。run は選んだ行を実行する。clear
// は選びを外す（Esc）。値が増えたら写し先の switch が落ちる（CPP-002）。
enum class StartScreenRoute : std::uint8_t
{
    pass,
    next,
    previous,
    run,
    clear
};
} // namespace nenenib::application
