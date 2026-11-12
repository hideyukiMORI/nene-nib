#pragma once

#include "TextPosition.hpp"
#include "VimChange.hpp"

#include <optional>

namespace nenenib::core
{
// engine が鍵の効果に添える変更の印（ADR 0181 の決定 1・追補 #1374）。kind は Vim が u_save と
// changed_* を呼んだか、at は kind が changed のときに Vim が changed_* に渡した位置（行と桁・鍵の
// 前の本文の座標で、変えた範囲の始まりより前は鍵の後も同じ）。saved_only は位置を持たない。
// 窓の先頭を決め直すか（Vim の changed_common の式）は controller がこの位置と鍵の後のカーソルで
// 決める（#1374）。
struct VimChangeMark
{
    VimChange kind;
    std::optional<TextPosition> at;
};
} // namespace nenenib::core
