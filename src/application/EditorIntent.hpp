#pragma once

#include "CancelSelection.hpp"
#include "ClipboardAction.hpp"
#include "DeleteText.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "PlaceCaret.hpp"
#include "RefreshAppearance.hpp"
#include "ScrollLines.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "VisibleLines.hpp"

#include <variant>

namespace nenenib::application
{
// 窓が出せる意図の閉じた和型（ARC-011 / CPP-002）。std::variant は CPP-006 が禁じる汎用データ
// バッグではなく、選択肢が全部見えている閉じた集合なので使える。増えたら std::visit の
// オーバーロード集合が足りずコンパイルが落ちる＝網羅性はここでも機械が守る（ADR 0009）。
using EditorIntent = std::variant<InsertText, MoveCaret, PlaceCaret, DeleteText, NewLine, SelectAll,
                                  CancelSelection, ClipboardAction, HistoryAction, ScrollLines,
                                  VisibleLines, SelectEditMode, RefreshAppearance>;
} // namespace nenenib::application
