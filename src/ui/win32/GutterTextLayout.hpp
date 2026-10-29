#pragma once

#include "GutterAlignment.hpp"
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
    // 寄せも鍵（#1041）。同じ文字と寸法でも左寄せと右寄せは別の layout。
    core::GutterAlignment alignment;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
};
} // namespace nenenib::ui::win32
