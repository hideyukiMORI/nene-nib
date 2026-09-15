#pragma once

#include "AppearancePort.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "EditBoundary.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorState.hpp"
#include "FileFailure.hpp"
#include "FilePort.hpp"
#include "LineView.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "SelectionAnchoring.hpp"
#include "TextEncoding.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::application
{
// 状態遷移はここだけ（ARC-004 / ARC-011）。UI は apply の返す EditorFrame を写す。
// 意図は閉じた和型で、std::visit の写し先が足りなければコンパイルが落ちる（CPP-002 / ADR 0009）。
class EditorController final
{
  public:
    EditorController(const AppearancePort &appearance, ClipboardPort &clipboard, FilePort &files,
                     CodePagePort &code_pages);
    [[nodiscard]] EditorFrame apply(const EditorIntent &intent);
    [[nodiscard]] EditorFrame frame() const;

  private:
    void accept(const InsertText &intent);
    void accept(const MoveCaret &intent);
    void accept(const PlaceCaret &intent);
    void accept(const DeleteText &intent);
    void accept(const NewLine &);
    void accept(const SelectAll &);
    void accept(const CancelSelection &);
    void accept(const ClipboardAction &intent);
    void accept(const HistoryAction &intent);
    void accept(const ScrollLines &intent);
    void accept(const VisibleLines &intent);
    void accept(const SelectEditMode &intent);
    void accept(const RefreshAppearance &);
    void accept(const OpenDocument &intent);
    void accept(const SaveDocument &intent);

    void replace(const core::OffsetRange &range, std::string_view text,
                 core::EditBoundary boundary);
    void move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring);
    void follow_caret();
    void undo_edit();
    void redo_edit();
    void copy_selection();
    void cut_selection();
    void paste_clipboard();
    [[nodiscard]] std::vector<LineView> visible_lines() const;
    void fail(FileFailure failure);
    // バイト列 ↔ UTF-8 の本文。BOM の着脱はここ、CP932 の変換はポートの向こう（決定 2・5）。
    [[nodiscard]] std::expected<std::string, FileFailure> decoded(core::TextEncoding encoding,
                                                                  std::string_view bytes);
    [[nodiscard]] std::expected<std::string, FileFailure> encoded(core::TextEncoding encoding,
                                                                  std::string_view utf8);

    const AppearancePort &appearance_;
    ClipboardPort &clipboard_;
    FilePort &files_;
    CodePagePort &code_pages_;
    EditorState state_;
};
} // namespace nenenib::application
