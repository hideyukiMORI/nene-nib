#pragma once

#include "LayoutRect.hpp"

#include <cstdint>

namespace nenenib::core
{
// メニューの配置の入力（ADR 0078 の追補 #1051）。x と y は開く点（クライアント座標の物理画素）、
// bounds は収める矩形。name_width と key_width は項目の名前と鍵の表示の最も広い幅（物理画素・ui が
// 描画と同じ書式で測る）。
struct ContextMenuRequest
{
    std::int32_t x;
    std::int32_t y;
    LayoutRect bounds;
    std::int32_t name_width;
    std::int32_t key_width;
    std::uint32_t dpi;
};
} // namespace nenenib::core
