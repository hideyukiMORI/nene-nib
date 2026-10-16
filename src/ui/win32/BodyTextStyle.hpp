#pragma once

#include "LineWrapping.hpp"
#include "TextViewGeometry.hpp"

#include <string>

namespace nenenib::ui::win32
{
// DirectWrite資源の無効化キー。編集設定の所有ではなく、焼き込んだ書式の控え。
struct BodyTextStyle
{
    std::string family;
    float points;
    core::TextViewGeometry geometry;
    core::LineWrapping wrapping;
    float cell = 0.0F;
};
} // namespace nenenib::ui::win32
