#pragma once

#include "Appearance.hpp"
#include "RgbColor.hpp"
#include "RgbaColor.hpp"

namespace nenenib::core
{
// テーマ 1 つ分の表示トークン（docs/design/2026-09-15-look.md 第 3 節）。
// どのメンバーも単独で妥当な色なので公開 aggregate（ADR 0007 / ADR 0008）。
// 組み込みテーマの表は BuiltinTheme.hpp が持ち、将来のテーマファイルも同じ値型を作る。
struct Palette
{
    RgbColor background;
    RgbColor text;
    RgbColor muted;
    RgbColor gutter;
    RgbColor current_line;
    RgbaColor titlebar_tint;
    RgbColor tab_active;
    RgbColor status;
    RgbColor accent;
    RgbaColor selection;
    RgbColor toggle;
    RgbColor on_accent;
    RgbColor panel;
    RgbColor panel_border;
};

[[nodiscard]] Palette palette_for(Appearance appearance) noexcept;
} // namespace nenenib::core
