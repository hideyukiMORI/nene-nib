#pragma once

#include "IndentGuideStyle.hpp"
#include "RainbowColors.hpp"
#include "RgbColor.hpp"

namespace nenenib::core
{
// インデントガイドの線の色を決める材料（ADR 0193 の決定 5）。indent_guide_paint が 1 frame に 1 回
// 作り、線ごとの色は indent_guide_color が引く。plain と plain_strong は単色の線と強める段
// （Palette::panel_border と Palette::muted）、background
// は虹の色を薄める先（Palette::background）。 公開 aggregate（CPP-003）。
struct IndentGuidePaint
{
    IndentGuideStyle style;
    RainbowColors rainbow;
    RgbColor plain;
    RgbColor plain_strong;
    RgbColor background;
};
} // namespace nenenib::core
