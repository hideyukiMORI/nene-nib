#pragma once

#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// 本文の右クリックのメニューの文字組みを作る道具（ADR 0078 の追補 #1051）。
// - dwrite は layout を作る factory。
// - name は項目の名前（Ctrl+P の面の行の名前と同じ書式・UI の字体）、key は鍵の表示（面の行の
//   右端の鍵と同じ書式）。どちらも段落の中央・折り返さない。
// - dpi は保持の鍵と寸法の倍率。
// 書式は借用で、持ち主は renderer（試験では試験の関数）。公開 aggregate（CPP-003）。
struct ContextMenuText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *name;
    IDWriteTextFormat *key;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
