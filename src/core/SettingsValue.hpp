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
    caret_motion_time
};
} // namespace nenenib::core
