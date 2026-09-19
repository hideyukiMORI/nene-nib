#pragma once

#include "AppearancePort.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "CompositionView.hpp"
#include "EditBoundary.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "FileFailure.hpp"
#include "FilePort.hpp"
#include "LineView.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "SelectionAnchoring.hpp"
#include "TextEncoding.hpp"
#include "VimEffect.hpp"
#include "VimState.hpp"

#include <expected>
#include <optional>
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
    explicit EditorController(EditorPorts ports,
                              std::optional<OpenDocument> initial = std::nullopt);
    [[nodiscard]] EditorFrame apply(const EditorIntent &intent);
    [[nodiscard]] EditorFrame frame() const;
    [[nodiscard]] bool command_line_active() const noexcept;
    [[nodiscard]] bool command_palette_active() const noexcept;
    // 無名レジスタと Vim のモードは表示値に載らないので、fixture の再生だけがここを読む
    // （ADR 0012 の決定 7）。状態を変える口はここには無い。
    [[nodiscard]] const core::VimState &vim_state() const noexcept;

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
    void accept(const VimKeyPress &intent);
    void accept(const RefreshAppearance &);
    void accept(const OpenDocument &intent);
    void accept(const SaveDocument &intent);
    void accept(const AdjustFontSize &intent);
    void accept(const CommandText &intent);
    void accept(const EditCommand &intent);
    void accept(const SubmitCommand &);
    void accept(const CancelCommand &);
    void accept(const PasteCommand &);
    void accept(const OpenCommandPalette &);
    void accept(const ActivateCommandChoice &intent);
    void submit_palette(const core::CommandPalette &palette);
    void evaluate_command(std::string_view text);
    [[nodiscard]] std::optional<core::CommandLine> command_line_view() const;
    [[nodiscard]] std::optional<CommandPaletteView> command_palette_view() const;
    [[nodiscard]] bool persist_settings(core::EditorSettings settings);
    // IME の 3 つ（ADR 0014 の決定 3）。ComposeText と CancelComposition は本文にも履歴にも
    // 触らず、CommitText だけが既存の 1 本（replace / vim_step）を通って本文に入る。
    void accept(const ComposeText &intent);
    void accept(const CommitText &intent);
    void accept(const CancelComposition &);

    // VimEffect の写し先。選択肢が増えたら std::visit がここで足りずコンパイルが落ちる
    // （CPP-002 / ADR 0012 の決定 3）。どれも既存の 1 本の経路を呼ぶだけ（ARC-001）。
    void perform(const core::VimNoEffect &);
    void perform(const core::VimOpenCommandLine &);
    void perform(const core::VimMoveTo &effect);
    void perform(const core::VimNavigate &effect);
    void perform(const core::VimSelect &effect);
    void perform(const core::VimRemoveRange &effect);
    void perform(const core::VimRemoveLines &effect);
    void perform(const core::VimInsertString &effect);
    void perform(const core::VimNewLine &);
    void perform(const core::VimPutString &effect);
    void perform(const core::VimUndo &);
    void perform(const core::VimRedo &);
    // INSERT にいるあいだの編集は 1 つの undo 単位に吸収する（ADR 0015 の決定 5）。
    [[nodiscard]] core::EditBoundary vim_boundary() const noexcept;

    void replace(const core::OffsetRange &range, std::string_view text,
                 core::EditBoundary boundary);
    void move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring);
    // NORMAL のキャレットは文字の上に置く。Vim の鍵のあとも、クリックと Ctrl+矢印のあとも、
    // 寄せ方はこの 1 本（決定 5）。通常モードと INSERT では何もしない。
    void settle_vim_caret();
    void follow_caret();
    void undo_edit();
    void redo_edit();
    void copy_selection();
    void cut_selection();
    void paste_clipboard();
    // 描く選択と Ctrl+C / Ctrl+X が覆う本文は同じ 1 本（ADR 0018 の決定 5）。
    [[nodiscard]] core::OffsetRange highlighted_range() const;
    [[nodiscard]] std::vector<LineView> visible_lines() const;
    [[nodiscard]] std::optional<CompositionView> composed() const;
    // Vim の NORMAL では IME を切ってあるので変換は来ないはずだが、来たら捨てる（決定 4）。
    [[nodiscard]] bool composition_ignored() const noexcept;
    // 確定した文字列を Vim の打鍵として流す。`.` の再生とマクロが後で自然に載る（決定 4）。
    void type_as_vim_keys(std::string_view utf8);
    void fail(FileFailure failure);
    // バイト列 ↔ UTF-8 の本文。BOM の着脱はここ、CP932 の変換はポートの向こう（決定 2・5）。
    [[nodiscard]] std::expected<std::string, FileFailure> decoded(core::TextEncoding encoding,
                                                                  std::string_view bytes);
    [[nodiscard]] std::expected<std::string, FileFailure> encoded(core::TextEncoding encoding,
                                                                  std::string_view utf8);

    EditorPorts ports_;
    EditorState state_;
};
} // namespace nenenib::application
