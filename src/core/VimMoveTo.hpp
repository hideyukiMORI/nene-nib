#pragma once

#include "Offset.hpp"

namespace nenenib::core
{
// キャレットを動かすだけの効果。選択は畳む（Vim には VISUAL まで選択が無い）。
struct VimMoveTo
{
    Offset caret;
};
} // namespace nenenib::core
