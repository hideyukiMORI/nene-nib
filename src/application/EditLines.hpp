#pragma once

#include "FormattingPieces.hpp"
#include "VimBlockEdit.hpp"
#include "VimLineMarks.hpp"

#include <span>
#include <variant>

namespace nenenib::application
{
// replace がマーク（ともう 1 つの枠）を直す形。行の規則（VimLineMarks）・矩形の行ごとの列
// （apply_block・#1361）・整形の編集の列（FormattingPieces・#1997 のレビュー R1）。
using EditLines =
    std::variant<core::VimLineMarks, std::span<const core::VimBlockEdit>, FormattingPieces>;
} // namespace nenenib::application
