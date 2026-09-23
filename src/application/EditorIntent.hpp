#pragma once

#include "ActivateCommandChoice.hpp"
#include "AdjustFontSize.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CancelSelection.hpp"
#include "ClipboardAction.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "DeleteText.hpp"
#include "EditCommand.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PasteCommand.hpp"
#include "PlaceCaret.hpp"
#include "RefreshAppearance.hpp"
#include "SaveDocument.hpp"
#include "ScrollLines.hpp"
#include "SearchHop.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"

#include <variant>

namespace nenenib::application
{
// 窓が出せる意図の閉じた和型（ARC-011 / CPP-002）。std::variant は CPP-006 が禁じる汎用データ
// バッグではなく、選択肢が全部見えている閉じた集合なので使える。増えたら std::visit の
// オーバーロード集合が足りずコンパイルが落ちる＝網羅性はここでも機械が守る（ADR 0009）。
using EditorIntent =
    std::variant<InsertText, MoveCaret, PlaceCaret, DeleteText, NewLine, SelectAll, CancelSelection,
                 ClipboardAction, HistoryAction, ScrollLines, VisibleLines, SelectEditMode,
                 VimKeyPress, RefreshAppearance, OpenDocument, SaveDocument, ComposeText,
                 CommitText, CancelComposition, AdjustFontSize, CommandText, EditCommand,
                 SubmitCommand, CancelCommand, PasteCommand, OpenCommandPalette,
                 ActivateCommandChoice, SearchHop>;
} // namespace nenenib::application
