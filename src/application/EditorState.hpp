#pragma once

#include "Appearance.hpp"
#include "AutosaveId.hpp"
#include "CommandInput.hpp"
#include "Composition.hpp"
#include "DisplayForm.hpp"
#include "DisplayInquiry.hpp"
#include "Document.hpp"
#include "DocumentReading.hpp"
#include "DocumentState.hpp"
#include "Edit.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ExplorerState.hpp"
#include "FileFailure.hpp"
#include "InputHistory.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"
#include "ParkedTab.hpp"
#include "ScrollState.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SettingsIssue.hpp"
#include "SplitLayout.hpp"
#include "SplitState.hpp"
#include "TabRecency.hpp"
#include "TextBuffer.hpp"
#include "TitleBarTarget.hpp"
#include "UnloadedDocument.hpp"
#include "VimMarks.hpp"
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
    [[nodiscard]] core::CaretAffinity caret_affinity() const noexcept;
    [[nodiscard]] EditorState with_caret_affinity(core::CaretAffinity affinity) const &;
    [[nodiscard]] EditorState with_caret_affinity(core::CaretAffinity affinity) &&;
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
    // `:` と検索の入力行の履歴（#725・ADR 0022 決定 2 の追補）。窓に 1 つずつで、タブの切り替え
    // では変わらない。足すのは controller の確定（submit）だけ。
    [[nodiscard]] const core::InputHistory &ex_history() const noexcept;
    [[nodiscard]] const core::InputHistory &search_history() const noexcept;
    [[nodiscard]] EditorState with_ex_history(core::InputHistory history) const;
    [[nodiscard]] EditorState with_search_history(core::InputHistory history) const;
    // エクスプローラー（ADR 0162 の決定 3）。窓に 1 つで、タブの切り替え・閉じる・開くでは
    // 変わらない。置くのは controller の explorer の群（EditorControllerExplorer.cpp）と、
    // Ctrl+P の面を開くときに焦点を本文へ戻す 1 か所だけ。
    [[nodiscard]] const ExplorerState &explorer() const noexcept;
    [[nodiscard]] EditorState with_explorer(ExplorerState explorer) const;
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
    // 裏で読んでいる最中の開く要求（ADR 0159 の決定 4・5）。窓に 1 つで、帯の外にある（タブの
    // 切り替え・閉じるでは変わらない）。置くのと消すのは controller の開く道と EndSession だけ。
    [[nodiscard]] const std::optional<DocumentReading> &reading() const noexcept;
    [[nodiscard]] EditorState with_reading(std::optional<DocumentReading> reading) const;
    [[nodiscard]] EditorState with_search_preview(std::optional<SearchPreview> preview) const;
    // 開いた本文で入れ替える。履歴は空・キャレットとスクロールは先頭に戻り、
    // モードと外観は保たれる（ADR 0010 の決定 8）。
    [[nodiscard]] EditorState with_opened(core::TextBuffer text, Document document) const;
    // 表示形式（ADR 0140 の決定 2・3）。アクティブな文書のもので、開いた文書・新しいタブは text。
    [[nodiscard]] core::DisplayForm display() const noexcept;
    [[nodiscard]] EditorState with_display(core::DisplayForm display) const;
    // 文書の言語（ADR 0154 の決定 1・5）。アクティブな文書のもの。開いた文書はパスから引き、
    // with_document でパスが変わったときも引き直す（`:set filetype=` の上書きは捨てる）。
    // with_language は `:set filetype=` の 1 か所（controller の accept(SelectLanguage)）。
    [[nodiscard]] core::Language language() const noexcept;
    [[nodiscard]] EditorState with_language(core::Language language) const;
    // 構文の色の入れ切り（ADR 0156 の追補・#829）。アクティブな文書のもので、開いた文書・新しい
    // タブは true。with_syntax_enabled は `:syntax on|off` の 1 か所（controller の
    // accept(SelectSyntax)）。
    [[nodiscard]] bool syntax_enabled() const noexcept;
    [[nodiscard]] EditorState with_syntax_enabled(bool enabled) const;
    // 開いたときの問いの段階（決定 2）。アクティブな文書のもの。進めるのは controller の
    // ask_display と accept(SelectDisplay) の 2 か所。
    [[nodiscard]] DisplayInquiry display_inquiry() const noexcept;
    // Vim のマーク（ADR 0152 の決定 2）。アクティブな文書のもので、置くのは controller の step_vim
    // （engine の値）と編集の 3 か所（replace・undo・redo）だけ。開いた文書は空。
    [[nodiscard]] const core::VimMarks &vim_marks() const noexcept;
    [[nodiscard]] EditorState with_vim_marks(core::VimMarks marks) const;
    [[nodiscard]] EditorState with_display_inquiry(DisplayInquiry inquiry) const;
    // 本文を仮想桁の索引を持つ写しに置き換える（ADR 0140 の決定 4）。本文の中身・履歴・選択は
    // 変えない。持たせるのは巨大な行の文書を開いたときと、格子にするときだけ。
    [[nodiscard]] EditorState with_virtual_columns() const;
    // 格子表示を聞く印（決定 2）。close_request と同じく 1 意図だけ立ち、with_intent_cleared
    // が消す。
    [[nodiscard]] bool display_question() const noexcept;
    [[nodiscard]] EditorState with_display_question(bool question) const;

    // タブ（ADR 0056 の決定 1・2）。上の欄と関数はどれも「アクティブな文書」のもので、ほかの
    // タブは不変の束として脇に置く。タブの本数は脇の束の数 + 1。
    [[nodiscard]] std::size_t tab_count() const noexcept;
    // 帯の上のアクティブの位置（0 始まり）。
    [[nodiscard]] std::size_t active_tab() const noexcept;
    // 脇に置いたタブ（帯の順・アクティブを除く）。読み込み済みの束か、まだ読んでいない文書か
    // （ADR 0059 の決定 4）。frame が表示値を並べ、契約が「切り替えても動いていないタブの束は同じ
    // 参照のまま」を見るための読み取りの口。
    [[nodiscard]] const std::vector<ParkedTab> &parked() const noexcept;
    // 帯の順の各タブの退避番号（ADR 0137 の決定 3）。タブを足す所（起動・NewTab・前回のタブの
    // 復元・自動退避から戻した文書）が通し番号を振り、閉じる・外すが列から抜く。切り替えは帯の順を変えないので触らない。
    // 番号は閉じても再利用しない。
    [[nodiscard]] const std::vector<AutosaveId> &autosave_ids() const noexcept;
    // 帯の位置 position がまだ読んでいない文書なら、その値。アクティブ・範囲の外・読み込み済みは
    // 値なし。読むのは controller の reach_tab だけ（ADR 0059 の決定 5）。
    [[nodiscard]] std::optional<UnloadedDocument> unloaded_at(std::size_t position) const;
    // 前回のタブ（ADR 0059 の決定 6）。今の帯の右へ、まだ読んでいない文書を順に並べる。使った順の
    // 列にも足し、アクティブは先頭のまま。アクティブは動かさない。
    [[nodiscard]] EditorState with_restored(std::vector<UnloadedDocument> documents) const;
    // 自動退避から戻した文書（ADR 0137 の決定 6）。今の帯の末尾へ、読み込み済みの束を順に並べる。
    // 使った順の列にも足し、アクティブは先頭のまま。アクティブは動かさない。
    [[nodiscard]] EditorState with_appended(std::vector<DocumentState> documents) const;
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

    // 枠（ADR 0148 の決定 2・3・5・9）。操作中の枠は上の欄そのもので、もう 1 つの枠は受け身の
    // 表示 PaneView として脇に 1 つだけ置く。layout == none ⇔ other が値なしで、1 枠の active_side
    // は first。
    [[nodiscard]] const SplitState &split() const noexcept;
    // 2 つの枠に分ける。もう 1 つの枠は今の文書を同じキャレット・スクロール・欲しい列で出し、
    // 操作中の枠は first の側（左か上）。none と既に 2 枠なら何も変えない。
    [[nodiscard]] EditorState with_split(core::SplitLayout layout) const;
    // もう 1 つの枠へ移る（決定 3）。同じ文書なら表示の欄（選択はキャレットに畳む・affinity・
    // スクロール・欲しい列）だけを入れ替える。違う文書なら with_switched と同じ置く・広げるの後に
    // 表示の欄を PaneView の値で置き替える。元の表示は新しいもう 1 つの枠になり、操作中の枠の側
    // （active_side）は反転する。使った順は文書が
    // 変わるときだけ直す。1 枠と、もう 1 つの枠がまだ読んでいない文書を指すとき（呼び手の誤り・
    // 読むのは controller の reach_tab）は何も変えない。
    [[nodiscard]] EditorState with_pane_swapped() const;
    // 1 枠に戻す（側は first）。文書には触れない。
    [[nodiscard]] EditorState without_split() const;
    // 操作中の枠の文書に当てる編集 edit（本文はまだ edit の前・after が edit の後の本文）を、同じ
    // 文書を出しているもう 1 つの枠のキャレットと先頭行に写す（決定 9）。キャレットは
    // offset_after_edit、先頭行は「その行の行頭の offset を offset_after_edit で写し、after で
    // 行番号に戻す」の 1 つの規則。違う文書を出しているとき・1 枠では何も変えない。undo は
    // inserted と removed を入れ替えた編集を渡す。
    [[nodiscard]] EditorState with_other_adjusted(const core::Edit &edit,
                                                  const core::TextBuffer &after) const;
    // もう 1 つの枠の先頭行と画面行を置く（ScrollPane）。1 枠では何も変えない。
    [[nodiscard]] EditorState with_other_scrolled(core::LineNumber first_visible,
                                                  std::size_t first_row) const;
    // もう 1 つの枠を操作中の枠の形で読む写し（決定 8）。本文・選択（キャレットに畳む）・
    // スクロール・表示形式がもう 1 つの枠のもので、変換・入力行・検索の preview は無い。frame と
    // ScrollPane が操作中の枠と同じ関数を通すための読み取りの口で、状態には戻さない。1 枠と、
    // まだ読んでいない文書を指すときは値なし。
    [[nodiscard]] std::optional<EditorState> other_pane_state() const;
    // もう 1 つの枠が受け身の本文の枠なら、その表示（ADR 0155 の決定 1）。無い・プレビューなら
    // nullptr。指す先は次に状態を写すまで。
    [[nodiscard]] const PaneView *other_pane() const noexcept;
    // もう 1 つの枠が Markdown のプレビューなら、その値（ADR 0155 の決定 1・2）。無い・本文の枠なら
    // nullptr。指す先は次に状態を写すまで。
    [[nodiscard]] const PreviewView *preview() const noexcept;
    // もう 1 つの枠にプレビュー view を置く（決定 3）。1 枠なら左右に分け（操作中の枠は first）、
    // 本文の枠なら置き換え（枠は 2 つまで・ADR 0148 の決定 1）、プレビューなら値を差し替える。
    // 配置と側は 2 枠のまま保つ。閉じるのは without_split。
    [[nodiscard]] EditorState with_preview(PreviewView view) const;

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
    // アクティブの今の表示を PaneView にする（with_split と with_pane_swapped の共通）。
    [[nodiscard]] PaneView active_pane() const;
    // view の表示の欄（キャレット・affinity・スクロール・欲しい列）を欄に広げる。
    void spread_pane(const PaneView &view);
    // もう 1 つの枠が tab を出していれば、アクティブの文書の先頭へ向け直す（決定
    // 5）。閉じたタブと、 本文が丸ごと替わるアクティブ（with_opened）の 2 か所。
    void restart_other_showing(AutosaveId tab);

    core::TextBuffer text_;
    core::Selection selection_;
    core::EditHistory history_;
    ScrollState scroll_;
    core::CaretAffinity caret_affinity_ = core::CaretAffinity::leading;
    core::DisplayForm display_ = core::DisplayForm::text;
    core::Language language_ = core::Language::plain;
    bool syntax_enabled_ = true;
    DisplayInquiry display_inquiry_ = DisplayInquiry::unjudged;
    bool display_question_ = false;
    core::Appearance appearance_;
    core::EditMode mode_;
    core::VimState vim_;
    Document document_;
    core::VimMarks vim_marks_{};
    std::optional<core::Composition> composition_;
    std::optional<FileFailure> last_failure_;
    core::EditorSettings settings_;
    core::ThemeCatalog themes_ = core::ThemeCatalog::builtins();
    std::optional<SettingsIssue> settings_failure_;
    std::optional<CommandInput> command_input_;
    std::optional<core::DisplayText> command_message_;
    std::optional<DocumentReading> reading_;
    std::optional<SearchPreview> search_preview_;
    core::InputHistory ex_history_ = core::InputHistory::empty();
    core::InputHistory search_history_ = core::InputHistory::empty();
    // 木は大きくなり得るので、状態の写し（with_* のたび）で複製しない不変の束で持つ（ADR 0056 の
    // 脇のタブと同じ形）。
    std::shared_ptr<const ExplorerState> explorer_ = std::make_shared<const ExplorerState>();
    std::vector<ParkedTab> parked_;
    std::size_t active_ = 0;
    std::vector<AutosaveId> autosave_ids_{AutosaveId{0}};
    std::uint64_t next_autosave_id_ = 1;
    core::TabRecency recency_ = core::TabRecency::single();
    bool tab_walk_ = false;
    bool closing_ = false;
    std::optional<std::size_t> close_request_;
    std::optional<core::EditorOperation> operation_request_;
    std::int32_t title_bar_width_ = 0;
    std::int32_t tab_scroll_ = 0;
    std::optional<core::TitleBarTarget> hovered_;
    SplitState split_;
};
} // namespace nenenib::application
