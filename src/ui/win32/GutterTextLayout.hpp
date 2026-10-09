#pragma once

#include "LayoutRect.hpp"

#include <dwrite.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 行番号の文字組みだけ。原点と色は描くたびに与える（ADR 0083）。
struct GutterTextLayout final
{
    std::string text;
    core::LayoutRect area;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
};
} // namespace nenenib::ui::win32
