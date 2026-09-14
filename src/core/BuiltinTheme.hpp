#pragma once

#include "Palette.hpp"

#include <cstdint>
#include <utility>

namespace nenenib::core
{
// 組み込みテーマの閉じた一覧。将来のテーマファイルは同じ Palette を作る別の経路になる（ADR 0008）。
enum class BuiltinTheme : std::uint8_t
{
    ubuntu_aubergine,
    neutral_light
};

// 採用案の表（docs/design/2026-09-15-look.md 第 3 節）をそのまま写した唯一の場所。
// ダークの茄子色は施主決定 D11、アクセントの Ubuntu 橙は施主承認（2026-09-15）。
inline constexpr Palette ubuntu_aubergine_palette{
    .background = RgbColor{0x30, 0x0A, 0x24},
    .text = RgbColor{0xEE, 0xEE, 0xEC},
    .muted = RgbColor{0xB8, 0xA9, 0xB3},
    .gutter = RgbColor{0x7A, 0x66, 0x75},
    .current_line = RgbColor{0x3E, 0x1A, 0x32},
    .titlebar_tint = RgbaColor{RgbColor{0xFF, 0xFF, 0xFF}, 11},
    .tab_active = RgbColor{0x3B, 0x14, 0x30},
    .status = RgbColor{0x26, 0x07, 0x1D},
    .accent = RgbColor{0xE9, 0x54, 0x20},
    .selection = RgbaColor{RgbColor{0xE9, 0x54, 0x20}, 71},
    .toggle = RgbColor{0x4A, 0x1E, 0x3D},
    .on_accent = RgbColor{0xFF, 0xFF, 0xFF},
    .panel = RgbColor{0x3B, 0x14, 0x30},
    .panel_border = RgbColor{0x5A, 0x2A, 0x4C}};

inline constexpr Palette neutral_light_palette{
    .background = RgbColor{0xF4, 0xF5, 0xF7},
    .text = RgbColor{0x1B, 0x1F, 0x24},
    .muted = RgbColor{0x5C, 0x65, 0x70},
    .gutter = RgbColor{0x9A, 0xA3, 0xAD},
    .current_line = RgbColor{0xE6, 0xE8, 0xEC},
    .titlebar_tint = RgbaColor{RgbColor{0x00, 0x00, 0x00}, 9},
    .tab_active = RgbColor{0xFF, 0xFF, 0xFF},
    .status = RgbColor{0xE9, 0xEB, 0xEF},
    .accent = RgbColor{0xE9, 0x54, 0x20},
    .selection = RgbaColor{RgbColor{0xE9, 0x54, 0x20}, 56},
    .toggle = RgbColor{0xDF, 0xE3, 0xE8},
    .on_accent = RgbColor{0xFF, 0xFF, 0xFF},
    .panel = RgbColor{0xFF, 0xFF, 0xFF},
    .panel_border = RgbColor{0xD5, 0xD9, 0xE0}};

[[nodiscard]] constexpr Palette palette_of(BuiltinTheme theme) noexcept
{
    switch (theme)
    {
    case BuiltinTheme::ubuntu_aubergine:
        return ubuntu_aubergine_palette;
    case BuiltinTheme::neutral_light:
        return neutral_light_palette;
    }
    std::unreachable();
}
} // namespace nenenib::core
