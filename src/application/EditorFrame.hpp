#pragma once

#include "Appearance.hpp"
#include "CaretView.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "Palette.hpp"
#include "StatusItems.hpp"

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::application
{
// UI が写すだけの表示値（ARC-011）。どのメンバーも検証済みの値なので公開 aggregate。
// lines は見えている行だけで、本文の全体は載らない（ADR 0009 の決定 6）。
struct EditorFrame
{
    std::vector<LineView> lines;
    CaretView caret;
    core::LineNumber first_visible;
    std::size_t total_lines;
    core::Appearance appearance;
    core::Palette palette;
    core::EditMode mode;
    std::string_view mode_label;
    core::DisplayText tab_title;
    std::array<core::DisplayText, core::status_item_count> status_items;
};
} // namespace nenenib::application
