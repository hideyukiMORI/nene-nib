#pragma once

#include <cstdint>

namespace nenenib::core
{
// 要素の文字の中の装飾の種類（ADR 0151 の決定 7）。リンクの行き先は持たない（開かない・M-5）。
// image は画像の代わりの文字「[画像: alt]」の全体に掛かる（画像は読まない・#784）。strikethrough は
// 打ち消し線（`~~a~~`・記号は文字に含めない・#1840）。alert_title は alert の 1 行目の種類の名前
// （太字・種類の色・#1848）。
enum class PreviewStyle : std::uint8_t
{
    emphasis,
    strong,
    code,
    link,
    image,
    strikethrough,
    alert_title
};
} // namespace nenenib::core
