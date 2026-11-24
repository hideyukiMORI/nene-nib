#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 選ぶ欄の一覧の配置の入力（#1926）。anchor は値の箱（settings_control_box）、bounds は一覧を出して
// よい矩形（設定の面）、count は選択肢の数、top は送り（いちばん上に見える選択肢の位置）、widest は
// いちばん長い選択肢の表示の名前の幅（物理画素・描画器が測る）。公開 aggregate（CPP-003）。
struct SettingsChoiceRequest
{
    LayoutRect anchor;
    LayoutRect bounds;
    std::uint32_t dpi;
    std::size_t count;
    std::size_t top;
    std::int32_t widest;
};
} // namespace nenenib::core
