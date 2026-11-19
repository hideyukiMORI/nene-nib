#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// プレビューが塗る役割（ADR 0151 の決定 17・採用案 D62 の案 B）。地は SyntaxPalette.background、
// 文字は SyntaxPalette の 6 役割、行内コードとコードブロックの面は Palette.panel。色の値は
// PreviewBrushes の switch の 1 か所が引く（ui/win32 に色のリテラルを書かない）。チェックボックスの
// 枠は Palette.muted、チェックの線は Palette.accent（ADR 0155 の追補
// 23）。全面プレビューの選択の面は 本文と同じ Palette.selection（不透明度つき・ADR 0155 の改訂
// 18）。
// 表の罫は Palette.panel_border、alert の線と名前は SyntaxPalette の function・string・number・
// warning・error（採用案 D103・#1848）。囲みのコードの構文の色は SyntaxPalette の 10 役割で、
// 足りなかった type と constant を末尾に足した（#1858・色のトークンは足していない）。表の横の送りの
// 棒の明るいつまみは Palette.text（#1883・役割を末尾に足しただけで色のトークンは足していない）。
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
    accent,
    selection,
    panel_border,
    number,
    warning,
    error,
    type,
    constant,
    text
};
} // namespace nenenib::ui::win32
