#pragma once

#include "EditorSettings.hpp"
#include "LineWindow.hpp"
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
    // 巨大な行の窓（ADR 0139・試作）。active なら layout は折り返さない。
    LineWindow window{};
};
} // namespace nenenib::application
