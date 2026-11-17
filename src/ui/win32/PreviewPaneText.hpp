#pragma once

#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// プレビュー欄の文字組みを作る道具（ADR 0189 の決定 5・採用案 docs/design/2026-11-16-finder-look.md
// の「プレビュー欄」）。
// - dwrite は layout を作る factory。
// - line は本文（本文と同じ族の字体・12.5 DIP・折り返さず・はみ出しは切る）、number は行番号（line
//   と同じ字体と大きさ・右寄せ）。
// - name は見出しの名前（UI の字体・11.5 DIP の semibold・末尾を文字単位の「…」で切る）、note は
//   見出しの場所・行・言語と中身を出せないときの 1 行（UI の字体・11.5 DIP）。
// - dpi は保持の鍵と寸法の倍率。
// 書式は借用で、持ち主は renderer（試験では試験の関数）。公開 aggregate（CPP-003）。
struct PreviewPaneText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *line;
    IDWriteTextFormat *number;
    IDWriteTextFormat *name;
    IDWriteTextFormat *note;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
