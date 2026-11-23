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

// 2 色の各成分の平均（切り捨て）。マウスを載せたタブの面は帯とアクティブなタブの間
// （ADR 0056 の決定 12）。
[[nodiscard]] constexpr RgbColor average(RgbColor left, RgbColor right) noexcept
{
    return RgbColor{static_cast<std::uint8_t>((left.red + right.red) / 2),
                    static_cast<std::uint8_t>((left.green + right.green) / 2),
                    static_cast<std::uint8_t>((left.blue + right.blue) / 2)};
}

// sRGB の 1 成分 → 線形の値の 100 万倍（WCAG 2.x の相対輝度の式・ADR 0017 の 2026-11-23 の追補）。
// v = c / 255 が 0.03928 以下なら v / 12.92、上なら ((v + 0.055) / 1.055)^2.4。
// 表はコンパイル時にだけ作る（consteval）。x^2.4 = x² × (x²)^(1/5) で、5 乗根は Newton 法
// （1 から上側で単調に収束する）。std::pow を呼ばないので libm のシンボルは出ない（ARC-003）。
[[nodiscard]] consteval double fifth_root(double value) noexcept
{
    double root = 1.0;
    for (int step = 0; step < 200; ++step)
    {
        const double fourth = root * root * root * root;
        root = (4.0 * root + value / fourth) / 5.0;
    }
    return root;
}

[[nodiscard]] consteval std::uint32_t srgb_linear_millionths_of(int channel) noexcept
{
    const double level = static_cast<double>(channel) / 255.0;
    const double base = (level + 0.055) / 1.055;
    const double linear = level <= 0.03928 ? level / 12.92 : base * base * fifth_root(base * base);
    return static_cast<std::uint32_t>(linear * 1'000'000.0 + 0.5);
}

[[nodiscard]] consteval std::array<std::uint32_t, 256> srgb_linear_millionths_table() noexcept
{
    std::array<std::uint32_t, 256> table{};
    for (std::size_t channel = 0; channel < table.size(); ++channel)
    {
        table.at(channel) = srgb_linear_millionths_of(static_cast<int>(channel));
    }
    return table;
}

inline constexpr std::array<std::uint32_t, 256> srgb_linear_millionths =
    srgb_linear_millionths_table();

// 数点は手で式を計算した値（四捨五入）と照らす。
static_assert(srgb_linear_millionths.at(0) == 0, "black is 0");
static_assert(srgb_linear_millionths.at(10) == 3'035, "10 / 255 is on the linear segment");
static_assert(srgb_linear_millionths.at(11) == 3'347, "11 / 255 is on the power segment");
static_assert(srgb_linear_millionths.at(128) == 215'861, "128 / 255 is about 0.2158605");
static_assert(srgb_linear_millionths.at(188) == 502'886, "188 / 255 is about 0.5028865");
static_assert(srgb_linear_millionths.at(255) == 1'000'000, "white is 1");

// 相対輝度 L の 10^10 倍（重み 2126・7152・722 の和は 10,000・成分は 10^6 倍）。
[[nodiscard]] constexpr std::uint64_t relative_luminance_scaled(RgbColor color) noexcept
{
    return 2'126ULL * srgb_linear_millionths.at(color.red) +
           7'152ULL * srgb_linear_millionths.at(color.green) +
           722ULL * srgb_linear_millionths.at(color.blue);
}

// 黒の比は (L + 0.05) / 0.05、白の比は 1.05 / (L + 0.05)。黒の比が白の比以上なのは
// (L + 0.05)² ≥ 1.05 × 0.05 = 0.0525 のとき（両辺は正なので割り算を掛け算に移せる）。
// L を 10^10 倍の整数 S で書くと (S + 5×10^8)² ≥ 5.25×10^18（uint64 に収まる）。
// 左辺は S について単調なので、満たす最小の S をしきいにする（約 0.17913 × 10^10）。
inline constexpr std::uint64_t on_accent_black_offset = 500'000'000ULL;
inline constexpr std::uint64_t on_accent_black_square = 5'250'000'000'000'000'000ULL;

[[nodiscard]] consteval std::uint64_t on_accent_black_threshold_scaled() noexcept
{
    std::uint64_t low = 0;
    // 上端 2×10^9 では (2.5×10^9)² = 6.25×10^18 で、uint64 の 1.8×10^19 に収まる。
    std::uint64_t high = 2'000'000'000ULL;
    while (low < high)
    {
        const std::uint64_t middle = low + (high - low) / 2;
        const std::uint64_t shifted = middle + on_accent_black_offset;
        if (shifted * shifted >= on_accent_black_square)
        {
            high = middle;
        }
        else
        {
            low = middle + 1;
        }
    }
    return low;
}

inline constexpr std::uint64_t on_accent_black_threshold = on_accent_black_threshold_scaled();

static_assert(on_accent_black_threshold == 1'791'287'848ULL,
              "sqrt(0.0525) - 0.05 is about 0.1791287847");

// アクセントの上の文字は、黒と白のうち WCAG 2.x のコントラスト比が大きい方（等しければ黒）。
[[nodiscard]] constexpr RgbColor on_accent_for(RgbColor accent) noexcept
{
    return relative_luminance_scaled(accent) >= on_accent_black_threshold ? mix_black : mix_white;
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
              "ui_mix_weights is indexed in the order of Appearance");
static_assert(static_cast<std::size_t>(Appearance::dark) == 1,
              "ui_mix_weights is indexed in the order of Appearance");

// 本文の 3 色と明暗から UI トークン 19 個を導く決定的な純関数（ADR 0017 / 0117）。
// 有名テーマは本文の色しか定義しないので、タイトルバー・ステータスバー・パネルはここが決める。
// 導出した見た目は仮で、C3 の絵で hide が見る（D12）。
[[nodiscard]] constexpr Palette derive_ui(RgbColor background, RgbColor foreground, RgbColor accent,
                                          Appearance appearance) noexcept
{
    const UiMixWeights weights = ui_mix_weights[static_cast<std::size_t>(appearance)];
    const RgbColor title_bar = mix(background, mix_black, weights.title_bar);
    return Palette{
        .background = background,
        .text = foreground,
        .muted = mix(foreground, background, weights.muted),
        .gutter = mix(foreground, background, weights.gutter),
        .gutter_background = background,
        .current_line = mix(background, foreground, weights.current_line),
        .title_bar = title_bar,
        .tab_active = background,
        .tab_hover = average(title_bar, background),
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
        .giant_mark = accent,
    };
}

// 組み込みの行番号欄の面と数字だけを、テーマの表で選んだ色へ写す（ADR 0117）。
[[nodiscard]] constexpr Palette with_gutter(Palette palette, RgbColor background,
                                            RgbColor foreground) noexcept
{
    palette.gutter_background = background;
    palette.gutter = foreground;
    return palette;
}
} // namespace nenenib::core
