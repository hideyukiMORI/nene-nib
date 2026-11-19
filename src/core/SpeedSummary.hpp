#pragma once

#include "EditorFeature.hpp"
#include "SettingsPreset.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 設定の面の要約の 1 行（ADR 0170 の決定 5・2026-11-18 の追補 3）。estimate_us は基準の 1 打鍵
// reference_single_key_us に、段の印のあるオンの機能の増分を足した見込み（その機械の実測では
// ない）。low medium high はオンの印のある機能の段ごとの数、heavy は medium 以上でオンの機能（表の
// 順）。preset は今の設定が当たる組で、どれにも当たらなければ無し（組の外）。
struct SpeedSummary
{
    std::uint32_t estimate_us;
    std::size_t low;
    std::size_t medium;
    std::size_t high;
    std::vector<EditorFeature> heavy;
    std::optional<SettingsPreset> preset;
};
} // namespace nenenib::core
