#pragma once

#include "Appearance.hpp"
#include "Document.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "FileFailure.hpp"
#include "LineEnding.hpp"
#include "ScrollState.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimState.hpp"

#include <optional>

namespace nenenib::application
{
// 本文・選択・履歴・スクロール・改行・外観・編集モード・Vim の状態・文書の唯一の所有者
// （ARC-004）。Vim の状態は値型で、次の状態を決めるのは core の純関数（ADR 0012 の決定 1）。
// 公開値は不変で、次状態を返す（ARC-005 / CPP-003）。生成経路は create ただ 1 つ（CPP-007）。
class EditorState final
{
  public:
    [[nodiscard]] static EditorState create(core::Appearance appearance, core::EditMode mode);

    [[nodiscard]] const core::TextBuffer &text() const noexcept;
    [[nodiscard]] const core::Selection &selection() const noexcept;
    [[nodiscard]] const core::EditHistory &history() const noexcept;
    [[nodiscard]] const ScrollState &scroll() const noexcept;
    [[nodiscard]] core::LineEnding line_ending() const noexcept;
    [[nodiscard]] core::Appearance appearance() const noexcept;
    [[nodiscard]] core::EditMode mode() const noexcept;
    [[nodiscard]] const core::VimState &vim() const noexcept;
    [[nodiscard]] const Document &document() const noexcept;
    [[nodiscard]] std::optional<FileFailure> last_failure() const noexcept;

    [[nodiscard]] EditorState with_appearance(core::Appearance appearance) const;
    [[nodiscard]] EditorState with_mode(core::EditMode mode) const;
    [[nodiscard]] EditorState with_vim(core::VimState vim) const;
    [[nodiscard]] EditorState with_selection(const core::Selection &selection) const;
    [[nodiscard]] EditorState with_scroll(const ScrollState &scroll) const;
    // 本文と選択と履歴は 1 つの編集で必ず一緒に動くので、まとめて次状態にする。
    [[nodiscard]] EditorState with_edit(core::TextBuffer text, const core::Selection &selection,
                                        core::EditHistory history) const;
    [[nodiscard]] EditorState with_history(core::EditHistory history) const;
    [[nodiscard]] EditorState with_document(Document document) const;
    [[nodiscard]] EditorState with_failure(std::optional<FileFailure> failure) const;
    // 開いた本文で入れ替える。履歴は空・キャレットとスクロールは先頭に戻り、
    // モードと外観は保たれる（ADR 0010 の決定 8）。
    [[nodiscard]] EditorState with_opened(core::TextBuffer text, core::LineEnding ending,
                                          Document document) const;

  private:
    EditorState(core::Appearance appearance, core::EditMode mode);

    core::TextBuffer text_;
    core::Selection selection_;
    core::EditHistory history_;
    ScrollState scroll_;
    core::LineEnding line_ending_;
    core::Appearance appearance_;
    core::EditMode mode_;
    core::VimState vim_;
    Document document_;
    std::optional<FileFailure> last_failure_;
};
} // namespace nenenib::application
