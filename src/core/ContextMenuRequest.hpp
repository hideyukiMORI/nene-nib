#pragma once

#include "ContextMenuKind.hpp"
#include "LayoutRect.hpp"

#include <cstdint>

namespace nenenib::core
{
// メニューの配置の入力（ADR 0078 の追補 #1051 / #1790）。x と y は開く点（クライアント座標の
// 物理画素）、bounds は収める矩形。name_width と key_width は項目の名前と鍵の表示の最も広い幅
// （物理画素・ui が描画と同じ書式で測る）。kind はメニューの種類（行と区切り線の並び）。
struct ContextMenuRequest
{
    std::int32_t x;
    std::int32_t y;
    LayoutRect bounds;
    std::int32_t name_width;
    std::int32_t key_width;
    std::uint32_t dpi;
    ContextMenuKind kind;
};
} // namespace nenenib::core
