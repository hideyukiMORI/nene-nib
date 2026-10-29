#pragma once

#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// エクスプローラーの面の名前の文字組みを作る道具（ADR 0162 の決定 8）。dwrite は layout を作る
// factory、format は UI の字体・本文の大きさ・文字単位の省略記号の書式、dpi は保持の鍵。どれも
// 借用で、持ち主は renderer（試験では試験の関数）。公開 aggregate（CPP-003）。
struct ExplorerText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *format;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
