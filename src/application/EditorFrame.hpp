#pragma once

#include "Appearance.hpp"
#include "CaretView.hpp"
#include "CompositionView.hpp"
#include "DocumentView.hpp"
#include "EditMode.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "Palette.hpp"
#include "StatusItems.hpp"
#include "VimMode.hpp"

#include <array>
#include <cstddef>
#include <optional>
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
    // Vim のモード。窓は NORMAL のあいだ IME を切るのにこれを読む（ADR 0014 の決定 5）。
    // 通常モードのときは意味を持たない（mode_label にも出ない）。
    core::VimMode vim_mode;
    std::string_view mode_label;
    // 変換中の文字列。キャレットの位置に差し込んで描く（ADR 0014 の決定 2）。
    std::optional<CompositionView> composition;
    DocumentView document;
    std::array<core::DisplayText, core::status_item_count> status_items;
};
} // namespace nenenib::application
