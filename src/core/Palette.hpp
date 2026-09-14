#pragma once

#include "Appearance.hpp"
#include "RgbColor.hpp"

namespace nenenib::core
{
// 外観から決まる表示色の組。どちらのメンバーも単独で妥当な色なので公開 aggregate（ADR 0007）。
struct Palette
{
    RgbColor background;
    RgbColor text;
};

[[nodiscard]] Palette palette_for(Appearance appearance) noexcept;
} // namespace nenenib::core
