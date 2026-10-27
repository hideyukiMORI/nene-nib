#pragma once

#include "LayoutRect.hpp"
#include "PaletteTextRole.hpp"

#include <cstdint>
#include <dwrite.h>
#include <string_view>

namespace nenenib::ui::win32
{
// 面の行の文字組みを引く鍵（#900）。format は同一性の比較だけに使い（字体と大きさ）、書式を
// 作り直す前に保持を全部捨てる。area は幅と高さだけを比べ、原点は鍵に入れない。色・選択の地は
// 描くたびに与えるので入れない。text は引く間だけ借りる。
struct PaletteRowKey final
{
    IDWriteTextFormat *format = nullptr;
    PaletteTextRole role = PaletteTextRole::plain;
    std::string_view text;
    core::LayoutRect area{};
    std::uint32_t dpi = 0;
};
} // namespace nenenib::ui::win32
