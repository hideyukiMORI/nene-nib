#pragma once

#include <cstdint>

namespace nenenib::core
{
// 要素の文字の中の装飾の種類（ADR 0151 の決定 7）。リンクの行き先は持たない（開かない・M-5）。
enum class PreviewStyle : std::uint8_t
{
    emphasis,
    strong,
    code,
    link
};
} // namespace nenenib::core
