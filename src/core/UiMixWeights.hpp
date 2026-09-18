#pragma once

#include <cstdint>

namespace nenenib::core
{
// derive_ui の百分率の表 1 行ぶん（ADR 0017 の決定 4 の表）。明暗で 1 行ずつ持ち、
// 寄せ先（背景・前景・黒・白・アクセント）は ThemeDerivation.hpp の式が決める。
// 公開 aggregate（ADR 0007 / CPP-003）。
struct UiMixWeights
{
    std::uint8_t muted;            // 前景 → 背景
    std::uint8_t gutter;           // 前景 → 背景
    std::uint8_t current_line;     // 背景 → 前景
    std::uint8_t toggle;           // 背景 → 前景
    std::uint8_t panel_foreground; // 背景 → 前景
    std::uint8_t panel_white;      // その結果 → 白（ライトは 100 ＝ 白そのもの）
    std::uint8_t panel_border;     // 背景 → 前景
    std::uint8_t title_bar;        // 背景 → 黒
    std::uint8_t status;           // 背景 → 黒
    std::uint8_t selection_alpha;  // アクセントの不透明度
    std::uint8_t search_white;     // アクセント → 白
    std::uint8_t search_alpha;     // その色の不透明度
    std::uint8_t ime;              // 前景 → アクセント
};
} // namespace nenenib::core
