#pragma once

#include "PointText.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::application
{
// 本文の右クリックのメニューを開く（ADR 0078 の追補 #1051）。x と y は開く点（クライアント座標の
// 物理画素）。pointed は右クリックの点の本文の位置（PointText と同じ形）で、鍵（Shift+F10・
// メニューキー）で開くときは無し。通常モードで点が選択の外なら、controller が開く前にキャレットを
// そこへ置く（Windows の標準・D82）。選択の中と Vim のモードは選択とキャレットを動かさない。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct OpenContextMenu
{
    std::int32_t x;
    std::int32_t y;
    std::optional<PointText> pointed;
};
} // namespace nenenib::application
