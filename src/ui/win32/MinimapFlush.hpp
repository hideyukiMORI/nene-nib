#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// ミニマップの bitmap の命令を流した結果（ADR 0171 の追補 #1449）。kept は成功、minimap は
// ミニマップの命令だけが失敗した（窓を閉じず bitmap の道を捨てて直接描く）、frame はミニマップの
// 前の命令が失敗した（今までどおり描画の失敗として renderer へ返す）、device_lost は device を
// 失った（今までどおり作り直す）。
enum class MinimapFlush : std::uint8_t
{
    kept,
    minimap,
    frame,
    device_lost
};
} // namespace nenenib::ui::win32
