#pragma once

#include "Document.hpp"
#include "DocumentView.hpp"
#include "EditHistory.hpp"
#include "LineNumber.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimCount.hpp"
#include "VimWantedColumn.hpp"

#include <optional>

namespace nenenib::application
{
// 脇に置いたタブの文書の束（ADR 0056 の決定 1）。アクティブな文書は EditorState が欄のまま持ち、
// 切り替えるときだけ欄をこの束にして置く。置いた後は変えないので、状態の写しは束を指す参照だけを
// 写す。view は置くときに 1 回だけ作るタブの表示値（題名・未保存の印）で、frame は並べるだけ。
// どの欄も単独で妥当な値なので公開 aggregate（CPP-003）。
struct DocumentState
{
    core::TextBuffer text;
    core::Selection selection;
    core::EditHistory history;
    Document document;
    core::LineNumber first_visible;
    std::optional<core::VimWantedColumn> wanted_column;
    std::optional<core::VimCount> scroll_lines;
    DocumentView view;
};
} // namespace nenenib::application
