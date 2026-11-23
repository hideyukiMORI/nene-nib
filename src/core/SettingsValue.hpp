#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の、オン・オフでない値の行（#454 の案の土台・ADR 0170 の決定 5）。値を上下に送る。
// font_family は字体の一覧（OS の port）が要るので、段 1 では表示だけ（送らない）。
enum class SettingsValue : std::uint8_t
{
    theme,
    ui_language,
    font_family,
    font_size,
    finder_preview,
    // キャレットの動きの時間（ADR 0192・段の上下・20 ms ずつ）。
    caret_motion_time,
    // インデントガイドの形（ADR 0193 の決定 4）。単色 / 虹と強める段は選択、色の数（3〜6）と段の幅
    // （2 / 4 / 8）は段の上下。
    indent_guide_style,
    indent_guide_colors,
    indent_guide_active,
    indent_guide_width
};
} // namespace nenenib::core
