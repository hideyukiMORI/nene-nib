#pragma once

#include "Appearance.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "Palette.hpp"
#include "StatusItems.hpp"

#include <array>
#include <string_view>

namespace nenenib::application
{
// UI が写すだけの表示値（ARC-011）。どのメンバーも検証済みの値なので公開 aggregate。
struct EditorFrame
{
    core::DisplayText text;
    core::Appearance appearance;
    core::Palette palette;
    core::EditMode mode;
    std::string_view mode_label;
    core::DisplayText tab_title;
    std::array<core::DisplayText, core::status_item_count> status_items;
};
} // namespace nenenib::application
