#pragma once

#include "LayoutRect.hpp"
#include "SettingsControl.hpp"
#include "SettingsItemKind.hpp"

#include <cstddef>

namespace nenenib::core
{
// 行の領域の 1 つの矩形（送った後の位置・#1836）。index は heading なら節の番号（宣言の順）、row
// なら見えている行の中の位置、command なら命令の行の番号。control は row のときだけ意味を持つ。
struct SettingsLayoutItem
{
    SettingsItemKind kind;
    LayoutRect box;
    std::size_t index;
    SettingsControl control;
};
} // namespace nenenib::core
