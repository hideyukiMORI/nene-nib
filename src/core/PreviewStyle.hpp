#pragma once

#include <cstdint>

namespace nenenib::core
{
// 要素の文字の中の装飾の種類（ADR 0151 の決定 7）。リンクの行き先は持たない（開かない・M-5）。
// image は画像の代わりの文字「[画像: alt]」の全体に掛かる（画像は読まない・#784）。
enum class PreviewStyle : std::uint8_t
{
    emphasis,
    strong,
    code,
    link,
    image
};
} // namespace nenenib::core
