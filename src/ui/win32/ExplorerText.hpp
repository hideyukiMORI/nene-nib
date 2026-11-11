#pragma once

#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// エクスプローラーの面の名前の文字組みを作る道具（ADR 0162 の決定 8・採用案 B の #1390）。dwrite は
// layout を作る factory、format は UI の字体（タブの帯の題名と同じ族と大きさ・本文の字の大きさに
// 追従しない）・文字単位の省略記号の書式、strong は同じ書式の
// semibold（ルート名とアクティブな文書の 行）、dpi は保持の鍵。どれも借用で、持ち主は
// renderer（試験では試験の関数）。公開 aggregate （CPP-003）。
struct ExplorerText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *format;
    IDWriteTextFormat *strong;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
