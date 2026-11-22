#pragma once

#include "LayoutRect.hpp"
#include "SettingsLayoutRow.hpp"
#include "SettingsTextWidths.hpp"

#include <cstdint>
#include <span>

namespace nenenib::core
{
// 設定の面の配置の入力（施主決定 D102・#1836）。描画と当たりが同じ値を渡す。
// - area: 面を置ける帯（窓の client の幅いっぱい・タイトルバーの下からステータスバーの上まで）。
// - rows: 見えている行（表の順・「言語サーバー」の節の行と畳みの 1 行はその後ろ・#1945）。節の
//   変わり目に見出しを置く。
// - widths: 札・組・ボタンの文字の幅。
// - scroll: 行の領域の送り（物理画素・上に隠れた高さ）。配置が 0 から上限へ切り詰める。
struct SettingsLayoutRequest
{
    LayoutRect area;
    std::uint32_t dpi;
    std::span<const SettingsLayoutRow> rows;
    SettingsTextWidths widths;
    std::int32_t scroll;
};
} // namespace nenenib::core
