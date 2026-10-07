#pragma once

#include "RgbColor.hpp"

namespace nenenib::ui::win32
{
// 本文案内と既存一覧の共通描画口に渡す、枠の寸法（DIP）と配色。
struct KeycapStyle
{
    float padding;
    float height;
    float radius;
    core::RgbColor face;
    core::RgbColor border;
    core::RgbColor text;
};
} // namespace nenenib::ui::win32
