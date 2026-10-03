#pragma once

#include "Offset.hpp"
#include "OrdinaryCharacterAction.hpp"

namespace nenenib::core
{
class TextBuffer;

// 通常モードの移動・着地・削除境界の正本。at は UTF-8 の code point 境界（ADR 0065）。
[[nodiscard]] Offset ordinary_character_boundary(const TextBuffer &text, Offset at,
                                                 OrdinaryCharacterAction action);
} // namespace nenenib::core
