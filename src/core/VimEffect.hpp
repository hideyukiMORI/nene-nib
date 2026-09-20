#pragma once

#include "VimInsertAt.hpp"
#include "VimInsertString.hpp"
#include "VimMoveTo.hpp"
#include "VimNavigate.hpp"
#include "VimNewLine.hpp"
#include "VimNoEffect.hpp"
#include "VimOpenCommandLine.hpp"
#include "VimRedo.hpp"
#include "VimRemoveLines.hpp"
#include "VimRemoveRange.hpp"
#include "VimSelect.hpp"
#include "VimUndo.hpp"

#include <variant>

namespace nenenib::core
{
// 1 つの鍵が起こすことの閉じた和型（ADR 0012 の決定 3 / CPP-006）。EditorController が
// std::visit で既存の move_caret_to / replace / undo_edit / redo_edit に写す。
// 選択肢が増えたら写し先が足りずコンパイルが落ちる＝網羅性は機械が守る（CPP-002）。
using VimEffect =
    std::variant<VimNoEffect, VimMoveTo, VimNavigate, VimSelect, VimRemoveRange, VimRemoveLines,
                 VimInsertString, VimNewLine, VimInsertAt, VimUndo, VimRedo, VimOpenCommandLine>;
} // namespace nenenib::core
