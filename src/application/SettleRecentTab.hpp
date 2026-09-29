#pragma once

namespace nenenib::application
{
// Ctrl を離した・窓がフォーカスを失った（ADR 0058 の決定 3・4）。歩いていれば今のタブを使った順の
// 先頭へ動かして歩きを終える。歩いていなければ何もしない。
struct SettleRecentTab
{
};
} // namespace nenenib::application
