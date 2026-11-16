#pragma once

#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// 全文検索の当たりの行の文字組みを作る道具（ADR 0167 の決定 5・採用案
// docs/design/2026-11-16-finder-look.md の「当たりの行」）。dwrite は layout を作る factory、line
// は 上の段（行の本文・本文と同じ族の字体・12.5 DIP）、place は下の段（相対パスと行・UI の字体・ 11
// DIP）の書式で、どちらも折り返さず末尾を文字単位の「…」で切る。dpi は保持の鍵。書式は借用で、
// 持ち主は renderer（試験では試験の関数）。公開 aggregate（CPP-003）。
struct ProjectHitText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *line;
    IDWriteTextFormat *place;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
