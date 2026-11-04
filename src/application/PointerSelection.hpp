#pragma once

#include "Selection.hpp"
#include "VimMode.hpp"

namespace nenenib::application
{
// 本文のダブルクリック・トリプルクリックが Vim の engine に置く VISUAL（#1250）。selection は
// VISUAL の選択（caret は含む側の端）、mode は visual か visual_line。鍵ではないので engine は
// vim_pointer_selected で受け、`.` とマクロの記録に積まない。どの値も単独で妥当な公開 aggregate。
struct PointerSelection
{
    core::Selection selection;
    core::VimMode mode;
};
} // namespace nenenib::application
