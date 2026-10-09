#pragma once

#include "Appearance.hpp"
#include "CommandInput.hpp"
#include "Composition.hpp"
#include "Document.hpp"
#include "DocumentState.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FileFailure.hpp"
#include "LineEnding.hpp"
#include "ParkedTab.hpp"
#include "ScrollState.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SettingsIssue.hpp"
#include "TabRecency.hpp"
#include "TextBuffer.hpp"
#include "TitleBarTarget.hpp"
#include "UnloadedDocument.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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
    // lvalue は元を保ち、所有権を渡した一時値は同じ更新経路で消費する（ADR 0081）。
    [[nodiscard]] EditorState with_vim(core::VimState vim) const &;
    [[nodiscard]] EditorState with_vim(core::VimState vim) &&;
    [[nodiscard]] EditorState with_selection(core::Selection selection) const &;
    [[nodiscard]] EditorState with_selection(core::Selection selection) &&;
    [[nodiscard]] EditorState with_scroll(ScrollState scroll) const &;
    [[nodiscard]] EditorState with_scroll(ScrollState scroll) &&;
    // 本文・選択・履歴・文書の保存位置を 1 つの編集で一緒に反映する。
    [[nodiscard]] EditorState with_edit(core::TextBuffer text, core::Selection selection,
                                        core::EditHistory history, Document document) const &;
    [[nodiscard]] EditorState with_edit(core::TextBuffer text, core::Selection selection,
                                        core::EditHistory history, Document document) &&;
    [[nodiscard]] EditorState with_history(core::EditHistory history) const &;
    [[nodiscard]] EditorState with_history(core::EditHistory history) &&;
    [[nodiscard]] EditorState with_document(Document document) const &;
    [[nodiscard]] EditorState with_document(Document document) &&;
    // 1 意図だけの失敗・終了・閉じる要求・操作要求を 1 回の写しで戻す。
    [[nodiscard]] EditorState with_intent_cleared() const;
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
    // 脇に置いたタブ（帯の順・アクティブを除く）。読み込み済みの束か、まだ読んでいない文書か
    // （ADR 0059 の決定 4）。frame が表示値を並べ、契約が「切り替えても動いていないタブの束は同じ
    // 参照のまま」を見るための読み取りの口。
    [[nodiscard]] const std::vector<ParkedTab> &parked() const noexcept;
    // 帯の位置 position がまだ読んでいない文書なら、その値。アクティブ・範囲の外・読み込み済みは
    // 値なし。読むのは controller の reach_tab だけ（ADR 0059 の決定 5）。
    [[nodiscard]] std::optional<UnloadedDocument> unloaded_at(std::size_t position) const;
    // 前回のタブ（ADR 0059 の決定 6）。今の帯の右へ、まだ読んでいない文書を順に並べる。使った順の
    // 列にも足し、アクティブは先頭のまま。アクティブは動かさない。
    [[nodiscard]] EditorState with_restored(std::vector<UnloadedDocument> documents) const;
    // 帯の位置 position のまだ読んでいない文書を、読み込み済みの束に置き換える（決定 5）。
    // アクティブ・範囲の外・読み込み済みなら何も変えない。
    [[nodiscard]] EditorState with_loaded(std::size_t position, DocumentState bundle) const;
    // 帯の位置 position のまだ読んでいない文書を、帯と使った順から外す（決定 5・D26・D27）。
    // アクティブ・範囲の外・読み込み済みなら何も変えない。
    [[nodiscard]] EditorState with_dropped(std::size_t position) const;
    // 使った順を順位から作り直す（決定 6）。ranks は帯の位置ごとの順位（0 がいちばん最近）で、
    // 本数が帯と違えば何も変えない。同じ順位は帯の順。
    [[nodiscard]] EditorState with_recency_ranked(std::span<const std::size_t> ranks) const;
    // 帯の位置 position のタブをアクティブにする。今の文書は束にして元の位置に置く（選択は
    // キャレットへ畳み、undo
    // の単位は閉じる）。範囲の外とまだ読んでいない文書なら何も変えず、アクティブ自身なら文書は
    // 動かさない（歩きの確定だけ・下の使った順）。
    [[nodiscard]] EditorState with_switched(std::size_t position) const;
    // 空の「無題」をアクティブの右に足してアクティブにする。今の文書は束にして置く。
    [[nodiscard]] EditorState with_new_tab() const;
    // 帯の位置 position のタブを捨てる。アクティブを捨てたら右隣（無ければ左隣）をアクティブに
    // する（決定 6）。範囲の外と最後の 1 つなら何も変えない（最後の 1 つは closing が受ける）。
    // アクティブを捨てて隣がまだ読んでいない文書のときも何も変えない（読むのは controller・
    // ADR 0059 の決定 5）。
    [[nodiscard]] EditorState with_closed(std::size_t position) const;
    // タブの使った順（ADR 0058 の決定 2）。上の 3 つ（切り替え・足す・閉じる）が列を直し、
    // 歩きを終える。with_switched はアクティブ自身を指されても歩きを終える（着いた所の確定）。
    [[nodiscard]] const core::TabRecency &recency() const noexcept;
    // Ctrl+Tab で歩いている間だけ真（決定 3）。
    [[nodiscard]] bool tab_walking() const noexcept;
    // 歩きの 1 歩。帯の位置 position のタブへ切り替えるが、使った順の列は入れ替えない。
    // 範囲の外とまだ読んでいない文書なら何も変えない。アクティブ自身なら歩いている印だけを立てる。
    [[nodiscard]] EditorState with_walked(std::size_t position) const;
    // 歩いていれば今のタブを使った順の先頭へ動かして歩きを終える。歩いていなければ同じ状態。
    [[nodiscard]] EditorState with_walk_settled() const;
    // 最後の 1 つのタブを閉じる意図の 1 回だけ立つ（D22）。last_failure と同じく次の意図で消す。
    [[nodiscard]] bool closing() const noexcept;
    [[nodiscard]] EditorState with_closing(bool closing) const;
    // Ex の `:tabclose` が閉じたいタブの位置（ADR 0057 の決定 6）。closing と同じく
    // 1 意図だけ立ち、状態は変えない（閉じるのは ui が × と同じ閉じる流れで行う）。
    [[nodiscard]] const std::optional<std::size_t> &close_request() const noexcept;
    [[nodiscard]] EditorState with_close_request(std::optional<std::size_t> position) const;
    // 操作の一覧で選ばれた操作（ADR 0078 の決定 8）。close_request と同じく 1 意図だけ立ち、
    // 状態は変えない（実行するのは ui の run_operation）。
    [[nodiscard]] const std::optional<core::EditorOperation> &operation_request() const noexcept;
    [[nodiscard]] EditorState
    with_operation_request(std::optional<core::EditorOperation> operation) const;
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
    // with_new_tab / with_closed / with_walked だけである。
    [[nodiscard]] std::shared_ptr<const DocumentState> parked_active() const;
    void spread(const DocumentState &tab);
    // 帯の位置 position（アクティブではない）の脇の列の添字。
    [[nodiscard]] std::size_t parked_index_of(std::size_t position) const noexcept;
    // 帯の位置 position（アクティブではない）が読み込み済みなら、その束。
    [[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
    loaded_at(std::size_t position) const;
    // 帯の位置 position の読み込み済みの束 target を広げる（使った順は触らない）。with_switched と
    // with_walked の共通。spread は読み込み済みの束しか受けない（ADR 0059 の決定 5）。
    [[nodiscard]] EditorState switched_to(std::size_t position, const DocumentState &target) const;
    // アクティブを使った順の先頭へ動かし、歩きを終える。使った順を直す 3 か所の締め。
    void touch_active();

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
    std::vector<ParkedTab> parked_;
    std::size_t active_ = 0;
    core::TabRecency recency_ = core::TabRecency::single();
    bool tab_walk_ = false;
    bool closing_ = false;
    std::optional<std::size_t> close_request_;
    std::optional<core::EditorOperation> operation_request_;
    std::int32_t title_bar_width_ = 0;
    std::int32_t tab_scroll_ = 0;
    std::optional<core::TitleBarTarget> hovered_;
};
} // namespace nenenib::application
