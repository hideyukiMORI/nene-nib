#pragma once

#include "EditorSettings.hpp"
#include "TextViewGeometry.hpp"

#include <string_view>

namespace nenenib::application
{
// 同期呼出しの間だけ借用する、完全な表示文字列と書式（ADR 0114）。
struct TextLayoutRequest
{
    std::string_view text;
    const core::EditorSettings &settings;
    core::TextViewGeometry geometry;
    // 格子の 1 升の物理画素（ADR 0140 の決定 5・試作）。0 は今日どおりの文字組み。
    float cell = 0.0F;
};
} // namespace nenenib::application
