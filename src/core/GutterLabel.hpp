#pragma once

#include "GutterAlignment.hpp"

#include <string>

namespace nenenib::core
{
// 行番号の欄に描く 1 行ぶんの文字と寄せ（#1041）。作るのは gutter_label の 1 本。
struct GutterLabel
{
    std::string text;
    GutterAlignment alignment;
};
} // namespace nenenib::core
