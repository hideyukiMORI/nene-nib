#pragma once

#include "Appearance.hpp"
#include "CommandInput.hpp"
#include "Composition.hpp"
#include "Document.hpp"
#include "DocumentState.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorSettings.hpp"
#include "FileFailure.hpp"
#include "LineEnding.hpp"
#include "ScrollState.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SettingsIssue.hpp"
#include "TextBuffer.hpp"
#include "TitleBarTarget.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

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
    // 改行の形は本文が持つ。状態は写しを持たない（判別の経路を 1 本にする・ADR 0036 の決定 1）。
    [[nodiscard]] core::LineEnding line_ending() const noexcept;
    [[nodiscard]] core::Appearance appearance() const noexcept;
    [[nodiscard]] core::EditMode mode() const noexcept;
    [[nodiscard]] const core::VimState &vim() const noexcept;
    [[nodiscard]] const Document &document() const noexcept;
    // 変換中の文字列は本文の外にある（ADR 0014 の決定 2）。変換していない間は空。
    [[nodiscard]] const std::optional<core::Composition> &composition() const noexcept;
    [[nodiscard]] std::optional<FileFailure> last_failure() const noexcept;
    [[nodiscard]] const core::EditorSettings &settings() const noexcept;
    [[nodiscard]] const core::ThemeCatalog &themes() const noexcept;
    [[nodiscard]] EditorState with_themes(core::ThemeCatalog themes) const;
    [[nodiscard]] std::optional<SettingsIssue> settings_failure() const noexcept;
    [[nodiscard]] const std::optional<CommandInput> &command_input() const noexcept;
    [[nodiscard]] const std::optional<core::DisplayText> &command_message() const noexcept;
    // 検索の入力行が開いているあいだだけ値を持つ（ADR 0041 の決定 2）。
    [[nodiscard]] const std::optional<SearchPreview> &search_preview() const noexcept;

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
    [[nodiscard]] EditorState with_composition(std::optional<core::Composition> composition) const;
    [[nodiscard]] EditorState with_failure(std::optional<FileFailure> failure) const;
    [[nodiscard]] EditorState with_settings(core::EditorSettings settings) const;
    [[nodiscard]] EditorState with_settings_failure(std::optional<SettingsIssue> failure) const;
    [[nodiscard]] EditorState with_command_input(std::optional<CommandInput> command) const;
    [[nodiscard]] EditorState with_command_message(std::optional<core::DisplayText> message) const;
    [[nodiscard]] EditorState with_search_preview(std::optional<SearchPreview> preview) const;
    // 開いた本文で入れ替える。履歴は空・キャレットとスクロールは先頭に戻り、
    // モードと外観は保たれる（ADR 0010 の決定 8）。
    [[nodiscard]] EditorState with_opened(core::TextBuffer text, Document document) const;

    // タブ（ADR 0056 の決定 1・2）。上の欄と関数はどれも「アクティブな文書」のもので、ほかの
    // タブは不変の束として脇に置く。タブの本数は脇の束の数 + 1。
    [[nodiscard]] std::size_t tab_count() const noexcept;
    // 帯の上のアクティブの位置（0 始まり）。
    [[nodiscard]] std::size_t active_tab() const noexcept;
    // 脇に置いた束（帯の順・アクティブを除く）。frame が表示値を並べ、契約が「切り替えても
    // 動いていないタブの束は同じ参照のまま」を見るための読み取りの口。
    [[nodiscard]] const std::vector<std::shared_ptr<const DocumentState>> &parked() const noexcept;
    // 帯の位置 position のタブをアクティブにする。今の文書は束にして元の位置に置く（選択は
    // キャレットへ畳み、undo の単位は閉じる）。範囲の外とアクティブ自身なら何も変えない。
    [[nodiscard]] EditorState with_switched(std::size_t position) const;
    // 空の「無題」をアクティブの右に足してアクティブにする。今の文書は束にして置く。
    [[nodiscard]] EditorState with_new_tab() const;
    // 帯の位置 position のタブを捨てる。アクティブを捨てたら右隣（無ければ左隣）をアクティブに
    // する（決定 6）。範囲の外と最後の 1 つなら何も変えない（最後の 1 つは closing が受ける）。
    [[nodiscard]] EditorState with_closed(std::size_t position) const;
    // 最後の 1 つのタブを閉じる意図の 1 回だけ立つ（D22）。last_failure と同じく次の意図で消す。
    [[nodiscard]] bool closing() const noexcept;
    [[nodiscard]] EditorState with_closing(bool closing) const;
    // 帯（ADR 0056 の決定 2）。帯の幅は ui が知らせる DIP で、まだ知らないあいだは 0。送り量は
    // DIP。マウスを載せている要素は、載せていなければ nullopt。
    [[nodiscard]] std::int32_t title_bar_width() const noexcept;
    [[nodiscard]] std::int32_t tab_scroll() const noexcept;
    [[nodiscard]] const std::optional<core::TitleBarTarget> &hovered() const noexcept;
    [[nodiscard]] EditorState with_title_bar_width(std::int32_t dip) const;
    [[nodiscard]] EditorState with_tab_scroll(std::int32_t dip) const;
    [[nodiscard]] EditorState with_hovered(std::optional<core::TitleBarTarget> hovered) const;

  private:
    EditorState(core::Appearance appearance, core::EditMode mode, DocumentState untitled);
    // 置く・広げるの 1 対（決定 2）。脇の束と欄の両方を知るのはこの 2 つと、上の with_switched /
    // with_new_tab / with_closed だけである。
    [[nodiscard]] std::shared_ptr<const DocumentState> parked_active() const;
    void spread(const DocumentState &tab);

    core::TextBuffer text_;
    core::Selection selection_;
    core::EditHistory history_;
    ScrollState scroll_;
    core::Appearance appearance_;
    core::EditMode mode_;
    core::VimState vim_;
    Document document_;
    std::optional<core::Composition> composition_;
    std::optional<FileFailure> last_failure_;
    core::EditorSettings settings_;
    core::ThemeCatalog themes_ = core::ThemeCatalog::builtins();
    std::optional<SettingsIssue> settings_failure_;
    std::optional<CommandInput> command_input_;
    std::optional<core::DisplayText> command_message_;
    std::optional<SearchPreview> search_preview_;
    std::vector<std::shared_ptr<const DocumentState>> parked_;
    std::size_t active_ = 0;
    bool closing_ = false;
    std::int32_t title_bar_width_ = 0;
    std::int32_t tab_scroll_ = 0;
    std::optional<core::TitleBarTarget> hovered_;
};
} // namespace nenenib::application
