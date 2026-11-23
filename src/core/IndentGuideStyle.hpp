#pragma once

#include <cstdint>

namespace nenenib::core
{
// インデントガイドの色の付け方（ADR 0193 の決定 4・5・settings.v6 の鍵 `indent_guide_style`）。
// rainbow は段ごとに rainbow_colors の色を巡回し、plain は Palette::panel_border の 1 色。
enum class IndentGuideStyle : std::uint8_t
{
    rainbow,
    plain
};
} // namespace nenenib::core
