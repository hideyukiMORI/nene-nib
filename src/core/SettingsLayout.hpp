#pragma once

#include "LayoutRect.hpp"
#include "SettingsLayoutItem.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace nenenib::core
{
// 設定の面の矩形（施主決定 D102 の右の引き出し・#1836）。描画と当たりの判定が同じ値を読む。
// - panel: 面。border は左の境の 1 DIP（全幅のときは幅 0）。
// - header: 見出しの行（題名と鍵の表示）。close は右上の ✕。search は探す欄。
// - chips: 節の札（宣言の順・折り返した後の位置）。divider は札の下の 1 DIP の線。
// - list: 行の領域（ここだけ縦に送る・描画の clip）。items は送った後の位置で、list の外へ出た
//   ものも含む。presets は要約の中の組のボタン（要約が無ければ空の矩形）。
// - footer_line: 下の帯の上の 1 DIP の線。note は「閉じると保存されます」、revert と close_button は
//   ボタン。
// - scroll: 切り詰めた後の送り。scroll_limit はその上限（行が入り切れば 0）。
// - narrow: 全幅（窓の幅が 700 DIP 未満）。`:set` の名前を描かない。
struct SettingsLayout
{
    LayoutRect panel;
    LayoutRect border;
    LayoutRect header;
    LayoutRect close;
    LayoutRect search;
    std::array<LayoutRect, 6> chips;
    LayoutRect divider;
    LayoutRect list;
    std::vector<SettingsLayoutItem> items;
    std::array<LayoutRect, 3> presets;
    LayoutRect footer_line;
    LayoutRect note;
    LayoutRect revert;
    LayoutRect close_button;
    std::int32_t scroll;
    std::int32_t scroll_limit;
    bool narrow;
    std::uint32_t dpi;
};
} // namespace nenenib::core
