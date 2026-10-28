#pragma once

#include "CaretAffinity.hpp"
#include "DisplayForm.hpp"
#include "DisplayInquiry.hpp"
#include "Document.hpp"
#include "DocumentView.hpp"
#include "EditHistory.hpp"
#include "Language.hpp"
#include "LineNumber.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimCount.hpp"
#include "VimMarks.hpp"
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
    std::size_t first_row = 0;
    float horizontal = 0;
    core::CaretAffinity affinity = core::CaretAffinity::leading;
    // 表示形式（ADR 0140 の決定 2・3）。文書ごとの値で、設定にもタブの一覧にも入れない。
    core::DisplayForm display = core::DisplayForm::text;
    // 開いたときの問いの段階（決定 2）。文書ごとに 1 回だけ判定する。
    DisplayInquiry inquiry = DisplayInquiry::unjudged;
    // Vim のマーク（ADR 0152 の決定 2）。文書の値で、まだ読んでいない文書・新しいタブは開いた時点
    // の値（前の位置だけ 1 行目・#962）。
    core::VimMarks marks{};
    // 文書の言語（ADR 0154 の決定 1）。表示形式と同じく文書ごとの値で、設定にもタブの一覧にも
    // 入れない。束を作る所がパスから引き（language_of）、`:set filetype=` の上書きは束に乗って
    // タブの切り替えに付いていく。
    core::Language language = core::Language::plain;
    // 構文の色の入れ切り（ADR 0156 の追補・#829）。言語と同じく文書ごとの値で、設定にもタブの一覧
    // にも入れない。開いた文書・まだ読んでいない文書を読んだ束・新しいタブは true。
    bool syntax_enabled = true;
};
} // namespace nenenib::application
