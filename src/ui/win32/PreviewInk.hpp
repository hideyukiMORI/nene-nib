#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// プレビューが塗る役割（ADR 0151 の決定 17・採用案 D62 の案 B）。地は SyntaxPalette.background、
// 文字は SyntaxPalette の 6 役割、行内コードとコードブロックの面は Palette.panel。色の値は
// PreviewBrushes の switch の 1 か所が引く（ui/win32 に色のリテラルを書かない）。チェックボックスの
// 枠は Palette.muted、チェックの線は Palette.accent（ADR 0155 の追補 23）。
enum class PreviewInk : std::uint8_t
{
    background,
    foreground,
    keyword,
    string,
    function,
    comment,
    operators,
    panel,
    muted,
    accent
};
} // namespace nenenib::ui::win32
