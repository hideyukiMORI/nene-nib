#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の 3 つの組（ADR 0170 の決定 5）。速さ優先 = 段 none だけオン・おすすめ = low まで
// オン・全部オン。組が触るのは段の印のある切れる機能だけ（2026-11-18 の追補 3）。
enum class SettingsPreset : std::uint8_t
{
    speed_first,
    recommended,
    all_on
};
} // namespace nenenib::core
