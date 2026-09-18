#pragma once

#include "Appearance.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"
#include "RgbaColor.hpp"
#include "UiMixWeights.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 補間の端点のうち、テーマに依らない 2 つ。
inline constexpr RgbColor mix_black{0x00, 0x00, 0x00};
inline constexpr RgbColor mix_white{0xFF, 0xFF, 0xFF};

// アクセントの上の文字を黒にする明るさの境目（ADR 0017 の決定 4）。
inline constexpr int on_accent_threshold = 160;

// sRGB の値のまま整数で補間する（ADR 0017 の決定 4）。ガンマも libm も持ち込まない
// （core から libm のシンボルを出すと ARC-003 が落ちる・ADR 0008 の決定 5）。
// 端数は 0 方向に落とす。知覚的に均一ではないことは ADR の「失うもの」に書いてある。
[[nodiscard]] constexpr std::uint8_t mix_channel(std::uint8_t from, std::uint8_t to,
                                                 std::uint8_t percent) noexcept
{
    const int start = static_cast<int>(from);
    const int distance = static_cast<int>(to) - start;
    return static_cast<std::uint8_t>(start + distance * static_cast<int>(percent) / 100);
}

[[nodiscard]] constexpr RgbColor mix(RgbColor from, RgbColor to, std::uint8_t percent) noexcept
{
    return RgbColor{mix_channel(from.red, to.red, percent),
                    mix_channel(from.green, to.green, percent),
                    mix_channel(from.blue, to.blue, percent)};
}

// アクセントの明るさ（299R + 587G + 114B）/ 1000 が境目以上なら黒、未満なら白。
[[nodiscard]] constexpr RgbColor on_accent_for(RgbColor accent) noexcept
{
    const int brightness =
        (299 * static_cast<int>(accent.red) + 587 * static_cast<int>(accent.green) +
         114 * static_cast<int>(accent.blue)) /
        1000;
    return brightness >= on_accent_threshold ? mix_black : mix_white;
}

// ADR 0017 の決定 4 の表そのもの。添字は Appearance（light = 0・dark = 1）で、
// 明暗の分岐はこの 1 か所にしかない（テーマ名の分岐は derive_ui の中に書かない・決定 5）。
// panel だけ寄せ先が明暗で違う（ダークは前景へ 6%・ライトは白へ 100%）ので、
// 「前景へ寄せてから白へ寄せる」1 つの式に畳んで百分率で書き分ける。
inline constexpr std::array<UiMixWeights, 2> ui_mix_weights{{
    {.muted = 30,
     .gutter = 55,
     .current_line = 7,
     .toggle = 9,
     .panel_foreground = 0,
     .panel_white = 100,
     .panel_border = 15,
     .title_bar = 8,
     .status = 4,
     .selection_alpha = 56,
     .search_white = 50,
     .search_alpha = 77,
     .ime = 25},
    {.muted = 30,
     .gutter = 55,
     .current_line = 7,
     .toggle = 14,
     .panel_foreground = 6,
     .panel_white = 0,
     .panel_border = 22,
     .title_bar = 38,
     .status = 20,
     .selection_alpha = 71,
     .search_white = 50,
     .search_alpha = 89,
     .ime = 25},
}};

static_assert(static_cast<std::size_t>(Appearance::light) == 0,
              "ui_mix_weights の添字は Appearance の並びそのもの");
static_assert(static_cast<std::size_t>(Appearance::dark) == 1,
              "ui_mix_weights の添字は Appearance の並びそのもの");

// 本文の 3 色と明暗から UI トークン 16 個を導く決定的な純関数（ADR 0017 の決定 4）。
// 有名テーマは本文の色しか定義しないので、タイトルバー・ステータスバー・パネルはここが決める。
// 導出した見た目は仮で、C3 の絵で hide が見る（D12）。
[[nodiscard]] constexpr Palette derive_ui(RgbColor background, RgbColor foreground, RgbColor accent,
                                          Appearance appearance) noexcept
{
    const UiMixWeights weights = ui_mix_weights[static_cast<std::size_t>(appearance)];
    return Palette{
        .background = background,
        .text = foreground,
        .muted = mix(foreground, background, weights.muted),
        .gutter = mix(foreground, background, weights.gutter),
        .current_line = mix(background, foreground, weights.current_line),
        .title_bar = mix(background, mix_black, weights.title_bar),
        .tab_active = background,
        .status = mix(background, mix_black, weights.status),
        .accent = accent,
        .selection = RgbaColor{accent, weights.selection_alpha},
        .toggle = mix(background, foreground, weights.toggle),
        .on_accent = on_accent_for(accent),
        .panel = mix(mix(background, foreground, weights.panel_foreground), mix_white,
                     weights.panel_white),
        .panel_border = mix(background, foreground, weights.panel_border),
        .search = RgbaColor{mix(accent, mix_white, weights.search_white), weights.search_alpha},
        .ime = mix(foreground, accent, weights.ime),
    };
}
} // namespace nenenib::core
