#pragma once

#include "LayoutRect.hpp"
#include "PaletteTextRole.hpp"

#include <cstdint>
#include <dwrite.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 面の行の 1 欄の保持（#900）。鍵は PaletteRowKey と同じ欄で、文字列は自分で持つ。used は
// 最後に引いた世代で、上限を越えたら最も小さいものを捨てる。
struct PaletteRowLayout final
{
    IDWriteTextFormat *format = nullptr;
    PaletteTextRole role = PaletteTextRole::plain;
    std::string text;
    core::LayoutRect area{};
    std::uint32_t dpi = 0;
    std::uint64_t used = 0;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
};
} // namespace nenenib::ui::win32
