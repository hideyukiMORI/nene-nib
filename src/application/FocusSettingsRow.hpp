#pragma once

#include <cstddef>

namespace nenenib::application
{
// 設定の面の焦点を見えている行の row 番目へ置く（行のクリック・Home / End・PageUp / PageDown・
// #1836）。見えている行の数を越えた値は最後の行（End は大きな値を送る）。行が無ければ何もしない。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct FocusSettingsRow
{
    std::size_t row;
};
} // namespace nenenib::application
