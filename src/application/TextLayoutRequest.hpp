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
};
} // namespace nenenib::application
