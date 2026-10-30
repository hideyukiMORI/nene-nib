#pragma once

#include "AppearancePort.hpp"
#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosaveRemnant.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "CommandChoice.hpp"
#include "CompositionView.hpp"
#include "DetectedText.hpp"
#include "Document.hpp"
#include "DocumentFormatKind.hpp"
#include "DocumentReadFinished.hpp"
#include "DocumentReadOutcome.hpp"
#include "DocumentReadProgress.hpp"
#include "DocumentReading.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "ExClipboardRequest.hpp"
#include "ExDocumentRequest.hpp"
#include "ExFormatRequest.hpp"
#include "ExLanguageRequest.hpp"
#include "ExLineRequest.hpp"
#include "ExLspRequest.hpp"
#include "ExMapRequest.hpp"
#include "ExPaletteRequest.hpp"
#include "ExResult.hpp"
#include "ExRootRequest.hpp"
#include "ExSyntaxRequest.hpp"
#include "ExTabRequest.hpp"
#include "ExWindowCommand.hpp"
#include "ExplorerEntry.hpp"
#include "ExplorerFolder.hpp"
#include "ExplorerView.hpp"
#include "FileBookmarks.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "FolderBatch.hpp"
#include "FontFamilies.hpp"
#include "GridLineMarks.hpp"
#include "GridRowView.hpp"
#include "GridTally.hpp"
#include "JsonValue.hpp"
#include "KeyMap.hpp"
#include "LanguageServerLink.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "LspNotification.hpp"
#include "LspRequest.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "MapTimer.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenFailureReply.hpp"
#include "OpenedDocument.hpp"
#include "PaletteFolder.hpp"
#include "PaletteOrigin.hpp"
#include "PaneSide.hpp"
#include "PaneSource.hpp"
#include "PendingLspRequest.hpp"
#include "ScrollState.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerEvent.hpp"
#include "ServerExited.hpp"
#include "ServerFailed.hpp"
#include "ServerLaunch.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "Session.hpp"
#include "SplitState.hpp"
#include "SyntaxStateCache.hpp"
#include "TabReach.hpp"
#include "TextBuffer.hpp"
#include "TextCaret.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TitleBarInput.hpp"
#include "UnloadedDocument.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimMapMode.hpp"
#include "VimMarks.hpp"
#include "VimPattern.hpp"
#include "VimRegister.hpp"
#include "VimRepeatFailure.hpp"
#include "VimRowSpan.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimStepSource.hpp"
#include "VisualTextView.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
// 状態遷移はここだけ（ARC-004 / ARC-011）。UI は apply の返す EditorDelivery を写す。
// 意図は閉じた和型で、std::visit の写し先が足りなければコンパイルが落ちる（CPP-002 / ADR 0009）。
class EditorController final
{
  public:
    // 起動引数のファイルは順にタブで開く（ADR 0056 の決定 13）。どれも通常の OpenDocument を通る。
    explicit EditorController(EditorPorts ports, const std::vector<OpenDocument> &initial = {});
    [[nodiscard]] EditorDelivery apply(const EditorIntent &intent);
    // 試験が意図を流して完全な描画値を読む薄い組み合わせ（ADR 0084）。
    [[nodiscard]] EditorFrame apply_frame(const EditorIntent &intent);
    // 本文の行を作らず、現在状態の共通反映欄だけを所有値として読む（ADR 0084）。
    [[nodiscard]] EditorDelivery delivery() const;
    // composition の有無だけを読む。本文と入力行のどちらに載せるかには依らない。
    [[nodiscard]] bool is_composing() const noexcept;
    // frame と同じ tab_views の薄い入口。本文の行は作らない。
    [[nodiscard]] std::vector<DocumentView> documents() const;
    [[nodiscard]] std::optional<CommandPaletteView> command_palette_view() const;
    // 試験の harness が Vim の鍵を 1 つ打つ口（ADR 0048 の決定 8）。窓の VimKeyPress と違い、
    // 入力行が開いていれば再生と同じ command_key の写しで入力行へ入る。
    [[nodiscard]] EditorFrame press_vim_key(const core::VimKey &key);
    // fixture の harness が :normal! の意味で流す口（Issue #230）。鍵を順に press_vim_key と
    // 同じに流し、鍵が閉じた失敗で終わったら残りの鍵を流さない。窓で打った鍵は失敗しても
    // 次の鍵が走るので、この口は通らない。
    [[nodiscard]] EditorFrame press_vim_keys(std::span<const core::VimKey> keys);
    [[nodiscard]] EditorFrame frame();
    [[nodiscard]] bool command_line_active() const noexcept;
    [[nodiscard]] bool command_palette_active() const noexcept;
    // エクスプローラーの面に焦点があるか（ADR 0162 の決定 4・E4 で足した口）。窓が鍵の行き先を
    // 決めるための安い読みで、frame を作らない。面が消えていれば偽。
    [[nodiscard]] bool explorer_focused() const noexcept;
    // 面の表示値（決定 3・8）。窓が押した行の木の位置（first_visible を足す）を決めるための口で、
    // frame の本文の行は作らない（command_palette_view と同じ形）。面が消えていれば値なし。
    [[nodiscard]] std::optional<ExplorerView> explorer_view() const;
    // 帯の配置の入力を状態の 4 つの値（タブの本数・アクティブの位置・送り量・マウスを載せた要素）
    // から読むだけの口。行も題名も作らない（ADR 0056 の決定 9）。窓の hit test と hover が
    // マウスの動くたびに呼ぶ。width は帯の幅（物理画素）。値は title_bar_input(frame(), ...)
    // と同じ。
    [[nodiscard]] core::TitleBarInput title_bar_input(std::int32_t width, std::uint32_t dpi) const;
    // 無名レジスタと Vim のモードは表示値に載らないので、fixture の再生だけがここを読む
    // （ADR 0012 の決定 7）。状態を変える口はここには無い。
    [[nodiscard]] const core::VimState &vim_state() const noexcept;
    // Ctrl+Tab で歩いている間だけ真（ADR 0058 の決定 4）。窓が Ctrl を離したとき・フォーカスを
    // 失ったときに SettleRecentTab を送るかを決めるための口で、状態の 1 欄を読むだけ。
    [[nodiscard]] bool tab_walking() const noexcept;
    // 枠の配置と操作中の枠の側（ADR 0148 の決定 8）。窓がマウスの点の下の枠と、本文の行数を
    // 数える枠（pane_layouts の片側）を引くための口で、状態の 1 欄を読むだけ。
    [[nodiscard]] const SplitState &split() const noexcept;
    // アクティブな文書の本文を読むだけの口。試験が仮想桁の索引の持ち方（ADR 0140 の決定 4）を
    // 確かめるために読む。状態を変える口はここには無い。
    [[nodiscard]] const core::TextBuffer &text() const noexcept;
    // 前回のプロセスが残した退避（ADR 0137 の決定 6）。ui が窓を見せて最初の frame を描いた後に
    // 1 回だけ呼ぶ。port の remnants() を 1 回呼んで列を覚え、その列を返す（失敗は空）。
    // RestoreAutosave と DiscardAutosave はこの覚えた列を使い、port へもう 1 回聞かない。
    [[nodiscard]] const std::vector<AutosaveRemnant> &autosave_remnants();

  private:
    // 意図の前に 1 意図ぶんだけの表示（失敗・報せ）を消す。keeps_message は報せを残す意図。
    void begin_intent(bool keeps_message);
    // Ctrl+Tab の歩きの途中に歩きを続けない入口が来たら、写す前に確定する（ADR 0058 の決定 4）。
    // keeps_walk は歩きを続ける意図か。Vim の鍵は歩きを続けない。
    void settle_tab_walk_before(bool keeps_walk);
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
    // 使う人が打った鍵の入口（ADR 0176 の決定 3）。Vim の `map`
    // の表を引いて、表に無い鍵はそのまま、 左辺の途中は積んで待ち、決まった鍵の列は順に
    // deliver_vim_key へ流す。表が空なら分岐 1 つで
    // deliver_vim_key。再生（`.`・マクロ・press_vim_keys）と IME の確定はここを通らない。
    [[nodiscard]] std::optional<core::VimRepeatFailure> typed_vim_key(const core::VimKey &key);
    // key_map_step の答えごとの写し（std::visit・答えが増えたらコンパイルが落ちる）。waited は
    // 引く前に待ちがあったか（流して空にしたらタイマーを外す）。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    typed_step(core::KeyMapPass pass, const core::VimKey &key, bool waited);
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    typed_step(core::KeyMapPending pending, const core::VimKey &key, bool waited);
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    typed_step(core::KeyMapEmit emitted, const core::VimKey &key, bool waited);
    // 表を引くモード。Vim モードで入力行が閉じていて、1 鍵の引数を待っていないときだけ値がある。
    [[nodiscard]] std::optional<core::VimMapMode> key_map_mode() const noexcept;
    // 積んだ鍵を key_map_flush で流して空にする（MapTimeout と、待ちを保たない意図の入口）。
    void flush_key_map_pending();
    // 積んだ鍵を流さずに捨てる（窓が閉じていく EndSession だけ）。
    void discard_key_map_pending();
    // 待ちの合図（決定 4）。積んだ鍵が無ければ何も変えない（描き直さない）。
    void accept(const MapTimeout &);
    // 設定の `maps` と `mapleader` から表を作り直す。設定を置く 2 か所（起動・persist_settings）
    // だけが呼ぶ。作れない設定（読み手が拒むので届かない）は空の表。
    void refresh_key_map();
    // 再生の鍵の口（ADR 0048 の決定 8）。入力行が開いていれば窓と同じ入力行の intent へ、
    // 閉じていれば engine へ。失敗を返すのは engine の鍵だけ。
    [[nodiscard]] std::optional<core::VimRepeatFailure> deliver_vim_key(const core::VimKey &key);
    // 入力行が開いているときの 1 鍵の写し。写し漏れは std::visit / switch が落とす（CPP-002）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> command_key(const core::VimCharacter &key);
    [[nodiscard]] std::optional<core::VimRepeatFailure> command_key(core::VimSpecialKey key);
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    command_key(const core::VimSearchPattern &key);
    // 1 鍵を engine へ流して効果を写す唯一の経路（ADR 0030 の決定 7）。打った鍵も再生の鍵も
    // ここを通り、鍵が閉じた失敗で終わったら理由を返す（ADR 0046 の決定 3）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> step_vim(const core::VimKey &key);
    // engine の 1 歩（鍵の vim_step か `:N` の vim_line_jumped）を測って写す 1 本。
    // 要求の往復・マーク・効果・キャレットの寄せは鍵と同じ（Issue #847）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> stepped_vim(const VimStepSource &source);
    [[nodiscard]] std::expected<core::VimStep, TextLayoutFailure>
    measured_vim_step(const VimStepSource &source);
    // engine の要求（表示領域・行の画面行数・行の中の画面行）に 1 つ応えて view へ置く（ADR 0129 の
    // 決定 2・ADR 0144 の決定 2）。
    [[nodiscard]] std::expected<void, TextLayoutFailure>
    answered_vim_request(const core::VimStep &step, core::VimEditorView &view,
                         std::vector<core::VimLineRows> &rows,
                         std::vector<core::VimRowSpan> &spans);
    void load_vim_clipboard(const core::VimKey &key);
    void send_vim_clipboard(const std::optional<core::VimRegister> &written);
    // 再生のあいだに積んだ編集を 1 つの undo 単位に畳む（ADR 0046 の決定 3）。
    void merge_replayed_edits(const core::EditHistory &before, const core::TextBuffer &text);
    // 再生の中でアクティブな文書が替わるとき（ADR 0057 の決定 3）。出ていく文書に積んだ編集を
    // その文書の 1 単位に畳み、入った文書の今の履歴と本文を次の畳みの起点にする。undo の単位は
    // 文書ごとで、再生の外では何もしない。
    void close_replayed_unit();
    void reopen_replayed_unit();
    // 再生の中の再生の鍵を列の先頭へ差し込む。深さが上限を超えたら残りを捨てる（決定 4）。
    void queue_nested_replay(const std::vector<core::VimKey> &keys, std::size_t depth);
    void accept(const RefreshAppearance &);
    // 開く道の 1 本（ADR 0056 の決定 5・ADR 0060 の決定 7・ADR 0159 の決定
    // 1・3〜5）。同じファイルの タブへ切り替える → 読みの port に頼む。同期に揃えば adopt_read
    // で続け、裏へ回れば読み込み中の 要求を置いて戻る。同じファイルを読み込み中なら 2
    // 本目を頼まない。ほかの読み込み中の要求は 取り消す。開けなければタブを足さず、reply
    // の告げ方で告げる。
    void open_document(const core::FilePath &path, OpenFailureReply reply);
    // 読みの結果を受ける 1 本（決定 3・5）。読み込み中の要求の券のものだけを使う。進み具合は要求に
    // 置き、終わりは採る瞬間まで留める（追補 2）。採るのは settle_reads。
    void adopt_read(DocumentReadOutcome outcome);
    void adopt_progress(const DocumentReadProgress &progress);
    void adopt_finished(DocumentReadFinished finished);
    [[nodiscard]] std::optional<DocumentReadFinished> take_arrived(std::uint64_t ticket);
    // 歩き・入力行・面・IME の変換中・Vim の待ちの最中か（揃った結果を採らない・追補 2・5）。
    [[nodiscard]] bool reads_held() const noexcept;
    // 留めた結果を頼んだ順に採る（追補 2・4）。開く要求は open_read か answer_unopened へ、タブの
    // 読みは finish_tab_read へ渡す。
    void settle_reads();
    [[nodiscard]] bool settle_read(std::uint64_t ticket, bool open_waiting);
    void finish_reading(const core::FilePath &path, OpenFailureReply reply,
                        std::expected<OpenedDocument, FileFailure> document);
    void finish_reading(const core::FilePath &path, AutosaveId tab,
                        std::expected<OpenedDocument, FileFailure> document);
    void finish_tab_read(AutosaveId tab, std::expected<OpenedDocument, FileFailure> document);
    [[nodiscard]] std::optional<std::size_t> tab_position_of(AutosaveId tab) const;
    // 揃った文書を開く（決定 4）。待っている間に同じファイルのタブができていればそこへ切り替え、
    // 何も書いていない無題ならそこに開き、ほかは新しいタブを足す。
    void open_read(const core::FilePath &path, OpenedDocument opened);
    // 開けなかったことを呼び手ごとの今の規則で告げる（ADR 0060 の決定 7・ADR 0159 の決定 6）。
    void answer_unopened(const core::FilePath &path, FileFailure failure, OpenFailureReply reply);
    // 読み込み中の要求を取り消す（券の仕事をやめさせて要求と留めた結果を消す・決定 5）。
    // only_opens なら開く要求だけ、そうでなければタブの読みも。
    void cancel_readings(bool only_opens);
    // 揃う前に閉じたタブの読みを取り消す（追補 1）。
    void cancel_closed_tab_reads();
    // apply の終わりの後始末（閉じたタブの読みの取消と、留めた結果の採用・追補 1・2）。
    void follow_reads();
    // WorkCompleted の道で読みの結果を全部引いて採る（決定 9）。
    void collect_reads();
    // 読み込み中の 1 行（決定 4）。「読み込み中: <題名> 37%」。要求が無ければ値なし。
    [[nodiscard]] std::optional<core::DisplayText> reading_message() const;
    void accept(const OpenDocument &intent);
    // 一覧から選んだファイルを開く。失敗は 1 行の知らせで、not_found で出どころが履歴の候補の
    // ときだけ履歴から外す（ADR 0062 の決定 18）。
    void open_listed(const core::FilePath &path, std::optional<core::PaletteOrigin> origin);
    // 閉じたファイルを履歴に記録する（read → history_recorded を順に → write）。空なら何もしない。
    void remember(const std::vector<core::FilePath> &paths);
    void forget(const core::FilePath &path);
    void accept(const SaveDocument &intent);
    void accept(const AdjustFontSize &intent);
    void accept(const CommandText &intent);
    void accept(const EditCommand &intent);
    void accept(const ScrollPaletteList &intent);
    // 面と `:` の補完の面の見えている行数を窓の寸法に合わせる 1 か所（ADR 0150 の決定 1）。
    // 意図と鍵を写した後に呼ぶ。行数が同じなら何もしない。
    void fit_command_rows();
    void accept(const SubmitCommand &);
    void accept(const CancelCommand &);
    void accept(const PasteCommand &);
    // Ctrl+P（ADR 0060 の決定 6）。候補の列を入力が空で開き、先頭を選ぶ。
    void accept(const OpenCommandPalette &);
    // 開いているタブの一覧（「∨」と `:tabs`・ADR 0060 の決定 6）。Ctrl+P と同じ列を入力 `#` で
    // 開き、アクティブなタブの行を選ぶ。
    void accept(const OpenTabList &);
    // 操作の一覧（F1・ADR 0078 の決定 9）。`:ls` と同じ口で入力 `?` の面を開く。入力行や面が
    // 開いていれば、取り消してから `?` の面に置き換える。
    void accept(const OpenOperationList &);
    // 面を開く 2 つの意図の 1 本。前の面の残りを collect で捨て、券を進め、同じフォルダを頼む
    // （ADR 0062 の決定 14）。input と selected は CommandPalette::opened へ渡す。
    void open_palette(std::string_view input, std::size_t selected);
    // アクティブな文書のフォルダを頼む。無題とフォルダを取り出せないパスでは頼まない。
    void request_folder(const std::vector<core::CommandChoice> &entries);
    // folder にある、一覧にもう出ているファイル（開いているタブと履歴の候補 entries）。
    [[nodiscard]] std::vector<core::FilePath>
    listed_in_folder(const core::FilePath &folder,
                     const std::vector<core::CommandChoice> &entries) const;
    // path が folder の直下にあるか（ファイルのあるフォルダと folder を FilePort の規則で比べる）。
    [[nodiscard]] bool in_folder(const core::FilePath &path, const core::FilePath &folder) const;
    // 届いたファイルを同じフォルダの候補にする（出さない拡張子と一覧にもう出ているものを除く・
    // OS が返した順のまま）。
    void append_folder_choices(const std::vector<core::FilePath> &files,
                               std::vector<core::CommandChoice> &choices) const;
    // Ctrl+P の面の候補の列を作る 1 本（ADR 0060 の決定 6）。開いているタブ（帯の順）→ 履歴
    // （新しい順・タブと登録を除く）。タブの直後へ登録順のブックマークを合流する（ADR 0063）。
    [[nodiscard]] std::vector<core::CommandChoice>
    palette_entries(const FileBookmarks &bookmarks) const;
    [[nodiscard]] std::vector<core::CommandChoice>
    palette_tabs(const FileBookmarks &bookmarks) const;
    void append_bookmark_choices(const FileBookmarks &bookmarks,
                                 std::vector<core::CommandChoice> &entries) const;
    void accept(const ToggleBookmark &);
    [[nodiscard]] std::optional<core::FilePath> bookmark_target() const;
    [[nodiscard]] bool bookmark_toggle_allowed() const;
    void accept(const ActivateCommandChoice &intent);
    void accept(const SearchHop &intent);
    void accept(const StoreVimRegister &intent);
    // タブ（ADR 0056 の決定 3・4・6）。切り替えは leave_document → EditorState の置く・広げる →
    // enter_document の順で、どの意図もこの 1 本を通る。
    void accept(const NewTab &);
    void accept(const SwitchTab &intent);
    // 使った順の歩きと確定（ADR 0058 の決定 3）。歩きの 1 歩も上の切り替えと同じ 1 本を通る。
    void accept(const WalkRecentTab &intent);
    void accept(const SettleRecentTab &);
    void accept(const CloseTab &intent);
    // 窓が閉じていくときに前回のタブの一覧を書く（ADR 0059 の決定 3）。状態は変えない。
    void accept(const EndSession &intent);
    // 裏の仕事の合図（ADR 0062 の決定 7）。使う人の操作の途中の状態を動かさない。
    void accept(const WorkCompleted &);
    // エクスプローラー（ADR 0162 の決定 3〜7・EditorControllerExplorer.cpp）。4 つの意図と
    // ルートの選びの答えはどれも使う人の操作（歩きを確定し、知らせを消す）。
    void accept(const ToggleExplorer &);
    void accept(const ExplorerKey &intent);
    void accept(const PointExplorer &intent);
    void accept(const ScrollExplorer &intent);
    void accept(const ExplorerRootChosen &intent);
    // `:set root=.` の要求（決定 7）。
    void run_root_request(core::ExRootRequest request);
    // 設定 root へ書いて「root=<path>」を知らせる。書けなければ失敗の 1 行。
    void set_explorer_root(const core::FilePath &folder);
    void focus_explorer(bool focused);
    // 面の行数（決定 2・E4 の追補）。行の高さは本文と同じで、先頭の 1 行はルート名なので本文の
    // 見えている行数 − 1（最小 1）。
    [[nodiscard]] std::size_t explorer_page() const noexcept;
    void step_explorer(bool down);
    void select_explorer_row(std::size_t row);
    void release_explorer(const std::optional<core::FilePath> &released, std::size_t row);
    void click_explorer_row(std::size_t row);
    void open_explorer_folder(std::optional<std::size_t> row);
    void enter_explorer_row(std::optional<std::size_t> row);
    void collapse_explorer_row();
    // 設定が変わった後（persist_settings）と起動の 1 か所。木をルートに揃え、読むものを頼む。
    void sync_explorer();
    // FolderPort が今の誰かの仕事を走らせているか（エクスプローラーは 1 つずつ頼む）。
    [[nodiscard]] bool folder_busy() const noexcept;
    void request_explorer_folder();
    // FolderPort の collect の 1 本。エクスプローラーの分を木へ写し、全部を返す。
    [[nodiscard]] std::vector<FolderBatch> receive_folders();
    void adopt_explorer_batch(ExplorerFolder &pending, FolderBatch &batch);
    void place_explorer_entries(const core::FilePath &folder,
                                std::span<const core::ExplorerEntry> entries);
    // 表示形式（ADR 0140 の決定 2・3）。窓の問いの「はい」と Ex の `:set display=` が通る 1 本。
    void accept(const SelectDisplay &intent);
    // 言語（ADR 0154 の決定 4・5）。Ex の `:set filetype=` `:set ft=` が通る 1 本。
    void accept(const SelectLanguage &intent);
    void accept(const SelectSyntax &intent);
    // 保存の形（ADR 0010 の追補 #1049 の決定 1・2）。面・`:set fenc=` `:set ff=`・F1 の操作が通る 1
    // 本。
    void accept(const SelectDocumentFormat &intent);
    // 保存の形の候補の面（決定 2 の (a)・(c)）。候補は閉じた enum の全部で、今の値に印を付ける。
    void accept(const OpenDocumentFormatList &intent);
    [[nodiscard]] std::pair<std::vector<core::CommandChoice>, std::size_t>
    format_choices(core::DocumentFormatKind kind) const;
    // 枠（ADR 0148）。分ける・移る・閉じる・もう 1 つの枠を送る。
    void accept(const SplitPane &intent);
    void accept(const SwitchPane &);
    void accept(const ClosePane &);
    void accept(const ScrollPane &intent);
    // Markdown のプレビュー（ADR 0155）。開く・閉じるは使う人の操作（決定 3）、tick は間引きの
    // 合図で使う人の操作ではない（決定 8）。
    void accept(const TogglePreview &);
    void accept(const PreviewTick &);
    // チェックボックスの切り替え（ADR 0155 の追補 24）。使う人の操作で、本文の編集 1 回。
    void accept(const TogglePreviewTask &intent);
    // プレビューが開いていれば、今の本文の解析を頼む（決定 3・6・9）。fresh は操作中の文書が
    // 替わったときで、前の結果と知らせを捨てて追従に戻す。16 MiB を超える本文は頼まず知らせる。
    void request_preview(bool fresh);
    // プレビューが開いていれば今の券の結果を引く（決定 6・10）。WorkCompleted の道だけが呼ぶ。
    void collect_preview();
    // 本文が送られたら（前の frame と先頭行が違えば）プレビューを追従に戻す（決定 7）。frame の 1
    // か所。
    void follow_preview_on_scroll(core::LineNumber first_visible);
    // frame の入口のスクロールの読み（settled_scroll の後に follow_preview_on_scroll）。
    [[nodiscard]] ScrollState framed_scroll();
    // 2 枠で操作中の枠を閉じる（`:q` 系と `Ctrl-w c`・ADR 0148 の決定 5・ADR 0155 の決定 4）。
    void close_operated_pane();
    // 操作中の枠の編集を、同じ文書を出しているもう 1 つの枠に写す（決定 9）。
    void adjust_other(const core::Edit &edit, const core::TextBuffer &after);
    // 編集の始まりの行が覚えの行より前なら、その文書の構文の覚えを捨てる（ADR 0156 の追補 #827）。
    // 編集の 1 か所（replace・undo・redo）が本文を置く前に呼ぶ。
    void forget_syntax_before(const core::Edit &edit);
    // 開いた文書に格子表示を聞くかを、アクティブな文書ごとに 1 回だけ判定する（決定 2）。文書が
    // アクティブになった直後（開く・enter_document）と apply の終わりと起動の終わりから呼ぶ。
    // Ctrl+Tab で歩いている間は判定だけして聞かない（確定で聞く）。
    void ask_display();
    // 言語サーバー（ADR 0160 の決定 1・5〜7・9・追補）。本体は EditorControllerLanguageServer.cpp。
    // reconcile は apply と起動の終わりで、設定とアクティブな文書の言語から起きているべき子に
    // 合わせる（オフで子が居なければ分岐 1 つ）。tick は古さの合図で使う人の操作ではない。
    void reconcile_language_server();
    void consider_language_server(core::Language language);
    [[nodiscard]] std::optional<ServerLaunch> wanted_language_server(core::Language language) const;
    void start_language_server(core::Language language, ServerLaunch launch);
    void stop_language_server();
    void shut_language_server();
    void send_lsp_request(LspRequestKind kind, std::string_view method,
                          const core::JsonValue &params);
    void language_server_failed(std::string_view head);
    void collect_language_server();
    void receive_server(const ServerStarted &started);
    void receive_server(const ServerMessage &message);
    void receive_server(const ServerExited &exited);
    void receive_server(const ServerFailed &failed);
    void end_language_server();
    void receive_lsp(const core::LspResponse &response);
    void receive_lsp(const core::LspNotification &notification);
    void receive_lsp(const core::LspRequest &request);
    void accept(const LanguageServerTick &);
    void expire_lsp_request(const PendingLspRequest &request);
    void answer_lsp(core::ExLspRequest request);
    void end_language_server_session();
    // 自動退避の合図（ADR 0137 の決定 1・2）。WorkCompleted と同じく使う人の操作の途中の状態を
    // 動かさない。
    void accept(const AutosaveTick &);
    // 計画を回して store と discard を頼み、帳面を進める（決定 2）。合図と復元の直後が呼ぶ。
    void autosave_now();
    // 起動の問いの答え（決定 6）。戻すのは覚えた残骸を順に読み、開くのと同じ復号で帯の末尾に
    // 未保存のタブとして足し、最初に戻したタブをアクティブにする。読めない残骸は 1 行知らせて
    // 飛ばす。戻したタブは直後に退避を頼んでから forget する。どちらの答えも残骸を全部 forget
    // する。
    void accept(const RestoreAutosave &);
    void accept(const DiscardAutosave &);
    // 1 本の残骸を読んで復号した束。読めない・復号できなければ値なし。
    [[nodiscard]] std::optional<DocumentState> restored_bundle(const AutosaveRemnant &remnant);
    // 計画が store にした 1 タブを帯から探して頼む。まだ読んでいない文書は計画が store にしない。
    void store_planned(AutosaveId id);
    // 1 タブの本文を保存と同じ符号化で写して頼む（決定 4）。頼めたら真で、呼び手が帳面に
    // 改訂番号を覚える。
    [[nodiscard]] bool store_autosave(AutosaveId id, const core::TextBuffer &text,
                                      const Document &document, core::TextPosition caret);
    // worker の失敗を引き、成功するまで 1 回だけ知らせる（決定 4）。
    void collect_autosave();
    void notify_autosave_failure();
    void notify_autosave_oversized(AutosaveId id);
    // 帯（ADR 0056 の決定 2・3・8）。帯の幅・ホイール・マウスを載せた要素。
    void accept(const TitleBarWidth &intent);
    void accept(const ScrollTabs &intent);
    void accept(const PointTitleBar &intent);
    // 帯の配置の入力（96 DPI・幅は DIP・決定 8）と、送り量をアクティブなタブが見える所へ直す
    // 1 本（切り替え・新しいタブ・閉じる・開く・帯の幅のたび）。帯の幅がまだ 0 なら送り量は 0。
    [[nodiscard]] core::TitleBarInput title_bar_input() const;
    void reveal_active_tab();
    // 開く（決定 5）の枝の材料。同じファイルを開いているタブの帯の位置・何も書いていない無題か。
    [[nodiscard]] std::optional<std::size_t> open_tab_of(const core::FilePath &path) const;
    [[nodiscard]] bool blank_untitled() const;
    // 揃った本文でまだ読んでいないタブを束に置き換える。読めなければ帯から外して 1 行知らせる。
    [[nodiscard]] TabReach loaded_tab(std::size_t position,
                                      std::expected<OpenedDocument, FileFailure> document);
    // 出ていく前に文書の上の一時の値を閉じる（入力行・検索の preview・IME の変換中の文字列）。
    void leave_document();
    // 入ってきた文書のスクロールを今の窓の行数で収め、NORMAL のキャレットを文字の上へ寄せる。
    void enter_document();
    // まだ読んでいない文書を読む 1 本（ADR 0059 の決定 5・ADR 0159 の追補 1）。帯の位置 position が
    // 読み込み済みかアクティブなら reached。まだ読んでいない文書は読みの port に頼み、同期に揃えば
    // 束に置き換えて reached、読めなければ帯から外して 1 行知らせて dropped、裏へ回れば reading
    // （タブは帯に残り、揃ったらそのタブへ切り替える）。範囲の外は dropped（何もしない）。タブが
    // 切り替わる入口（SwitchTab・WalkRecentTab・CloseTab の後の隣・起動）は状態を動かす前に
    // これを呼ぶ。
    [[nodiscard]] TabReach reach_tab(std::size_t position);
    // アクティブを閉じて隣へ移る（隣を reach_tab で読む・1 本も残らなければ空の無題）。
    void close_active_tab();
    void close_toward_reading(std::size_t active, std::size_t neighbour);
    // 読めなかったタブの知らせ（ADR 0059 の「知らせの文言」）。同じ意図の中の件数を数える。
    void report_unreached(const core::FilePath &path);
    // 起動で前回のタブを戻す（ADR 0059 の決定 6 と「手順」）。ファイルの引数が無いときだけ。
    void restore_session();
    void restore_tabs(const Session &session);
    // 入力行の Enter の写し先（ADR 0032 の決定 3）。選択肢が増えたら std::visit がここで
    // 足りずコンパイルが落ちる（CPP-002）。検索だけが engine へ鍵を 1 つ送る。
    [[nodiscard]] std::optional<core::VimRepeatFailure> submit(const core::CommandLine &line);
    [[nodiscard]] std::optional<core::VimRepeatFailure> submit(const core::CommandPalette &palette);
    [[nodiscard]] std::optional<core::VimRepeatFailure> submit(const core::SearchLine &line);
    [[nodiscard]] std::optional<core::VimRepeatFailure> submitted_command();
    [[nodiscard]] std::optional<core::VimRepeatFailure> confirmed_command();
    void remember(const core::CommandLine &line);
    void remember(const core::SearchLine &line);
    void remember(const core::CommandPalette &palette);
    void submit_palette(const core::CommandPalette &palette);
    void evaluate_command(std::string_view text);
    [[nodiscard]] bool ran_ex_request(const core::ExResult &result);
    void apply_unsaved_ex(const core::ExResult &result);
    // `:set filetype=` と `:set filetype?` の写し先（ADR 0154 の決定 4）。どちらも今の言語を
    // `filetype=<名前>` の 1 行で知らせる。
    void answer_language(const core::ExLanguageRequest &request);
    [[nodiscard]] bool answer_current_value(const core::ExResult &result);
    void answer_syntax(const core::ExSyntaxRequest &request);
    void answer_format(const core::ExFormatRequest &request);
    void answer_clipboard(core::ExClipboardRequest request);
    // `:nmap` 系の要求を設定へ写して保存する（ADR 0176 の決定 6）。断りは 1 行の知らせ。
    void run_map_request(const core::ExMapRequest &request);
    // Ex のタブの命令の写し先（ADR 0057 の決定 5・6）。命令が増えたら switch が落ちる（CPP-002）。
    void run_tab_request(const core::ExTabRequest &request);
    // Ex の `:N` `:$` の写し先（ADR 0022 の追補・Issue #847）。engine の `{N}G` と同じ着地。
    void jump_to_line(const core::ExLineRequest &request);
    void run_window_command(core::ExWindowCommand command);
    // Ex のファイル命令も Ctrl+P と同じ面へ渡す（ADR 0064）。
    // 断りの知らせを立てて OS の警告音を 1 回鳴らす。鳴らすかを決める唯一の口で、呼ぶのは入力行の
    // 確定とファイル操作の失敗だけ。鍵の失敗と情報の知らせは with_command_message（ADR 0153）。
    void reject(core::DisplayText message);
    void run_palette_request(const core::ExPaletteRequest &request);
    // GUI と Ex の保存は同じ処理で、失敗の表示だけが入口ごとに違う（ADR 0066）。
    [[nodiscard]] std::expected<void, FileFailure> save_document(const SaveDocument &intent);
    [[nodiscard]] bool write_ex_document(const core::ExDocumentRequest &request);
    [[nodiscard]] bool write_ex_path(const core::FilePath &path,
                                     const core::ExDocumentRequest &request);
    [[nodiscard]] bool refuse_modified_quit();
    void run_document_request(const core::ExDocumentRequest &request);
    // アクティブな文書の表示値。frame と一覧の候補が同じ 1 本を使う。
    [[nodiscard]] DocumentView active_document_view() const;
    [[nodiscard]] std::optional<core::InputLineView> command_line_view() const;
    // 見えている設定（ADR 0150 の決定 4）。焦点の候補の命令を evaluate_ex で評価した配色を保存した
    // 設定に重ねた値で、保存はしない。焦点が無い・評価できなければ保存した設定。delivery と frame
    // はこれを読み、保存と Ex の評価の比較元は state_.settings() のまま。
    [[nodiscard]] core::EditorSettings shown_settings() const;
    // frame の知らせ（ADR 0062 の決定 17）。状態の知らせが先で、無ければ面が開いていて今の券が
    // 打ち切られたときの 1 行。判断はここだけ（ARC-001）。
    [[nodiscard]] std::optional<core::DisplayText> command_message() const;
    [[nodiscard]] bool persist_settings(core::EditorSettings settings);
    // IME の 3 つ（ADR 0014 の決定 3）。ComposeText と CancelComposition は本文にも履歴にも
    // 触らず、CommitText だけが既存の 1 本（replace / vim_step）を通って本文に入る。
    void accept(const ComposeText &intent);
    void accept(const CommitText &intent);
    void accept(const CancelComposition &);
    // F5 の日時の挿入（ADR 0175 の決定 3）。時刻はここだけで ClockPort から読む（決定 4）。入力行・
    // 通常モード・Vim INSERT は IME の確定と同じ道で入れ、Vim のほかのモードは 1 行知らせる。
    void accept(const InsertDateTime &);

    // VimEffect の写し先。選択肢が増えたら std::visit がここで足りずコンパイルが落ちる
    // （CPP-002 / ADR 0012 の決定 3）。どれも既存の 1 本の経路を呼ぶだけ（ARC-001）。
    void perform(const core::VimNoEffect &);
    void perform(const core::VimOpenCommandLine &);
    void perform(const core::VimOpenSearch &effect);
    void perform(const core::VimMoveTo &effect);
    void perform(const core::VimNavigate &effect);
    void perform(const core::VimSelect &effect);
    void perform(const core::VimRemoveRange &effect);
    void perform(const core::VimRemoveLines &effect);
    void perform(const core::VimInsertString &effect);
    void perform(const core::VimNewLine &);
    void perform(const core::VimInsertAt &effect);
    void perform(const core::VimReplaceRange &effect);
    // 矩形の 3 つ（ADR 0035 の決定 3）。行ごとの置き換えを 1 つの Edit に畳んで写すので、
    // undo は矩形 1 つで 1 単位になる。
    void perform(const core::VimRemoveBlock &effect);
    void perform(const core::VimReplaceBlock &effect);
    void perform(const core::VimInsertBlock &effect);
    void apply_block(const std::vector<core::VimBlockEdit> &edits, core::Offset caret,
                     core::EditBoundary boundary);
    // 矩形の INSERT の Esc（ADR 0035 の追補 #1042）。写した編集を 1 行目の INSERT の単位に畳む。
    void fold_block_into_insert(const std::vector<core::VimBlockEdit> &edits, core::Offset caret);
    // `.` の再生（ADR 0030 の決定 7）。鍵を同じ accept の経路へ流すだけで、`.` は記録されない
    // ので再帰は深さ 1 で止まる。UI・IME・描画は通らない。
    void perform(const core::VimReplay &effect);
    void interrupt_vim_insert();
    void perform(const core::VimUndo &);
    void perform(const core::VimRedo &);
    // `gt` `gT`（ADR 0057 の決定 3）。窓の SwitchTab と同じ accept の 1 本へ写す。
    void perform(const core::VimSwitchTab &effect);
    // `Ctrl-w` の窓の命令（ADR 0148 の決定 7）。2 枠の意図が入るまで何もしない。
    void perform(const core::VimWindowCommand &command);
    // INSERT にいるあいだの編集は 1 つの undo 単位に吸収する（ADR 0015 の決定 5）。
    [[nodiscard]] core::EditBoundary vim_boundary() const noexcept;

    void replace(const core::OffsetRange &range, std::string_view text,
                 core::EditBoundary boundary);
    void move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring,
                       core::CaretAffinity affinity = core::CaretAffinity::leading);
    void accept(const PointText &intent);
    [[nodiscard]] bool move_visual(const MoveCaret &intent);
    // 画面を動かさずにキャレットを見えている範囲へ入れた位置（#785）。
    [[nodiscard]] std::expected<core::TextCaret, TextLayoutFailure> caret_in_view();
    [[nodiscard]] VisualTextView visual_view();
    void clamp_scroll();
    // NORMAL のキャレットは文字の上に置く。Vim の鍵のあとも、クリックと Ctrl+矢印のあとも、
    // 寄せ方はこの 1 本（決定 5）。通常モードと INSERT では何もしない。
    void settle_vim_caret();
    // 追従は頼むだけで、スクロールを読む直前に 1 回だけ解決する（ADR 0128 の決定 1・2）。
    void follow_caret();
    void follow_position(core::TextPosition position);
    void settle_follow();
    // 問いの間に文書を離れるときは保留を捨てる（ADR 0128 の決定 7）。
    void drop_pending_follow();
    // controller がスクロールを読む道はこの 1 本（ADR 0128 の決定 2）。
    [[nodiscard]] const ScrollState &settled_scroll();
    // incsearch の preview（ADR 0041 の決定 3・5）。
    void update_search_preview();
    void restore_search_origin(const ScrollState &origin);
    void undone_marks(const core::Edit &edit);
    void check_vim_marks();
    void adopt_vim_marks(const std::optional<core::VimMarks> &marks);
    void undo_edit();
    void redo_edit();
    // クリップボードへ書く唯一の口。書けなければ 1 行で知らせて false（ADR 0134 の決定 3）。
    [[nodiscard]] bool clipboard_written(std::string_view text);
    void copy_selection();
    void cut_selection();
    // 貼り付け（ADR 0157）。Ctrl+V は文書の言語で貼り方が決まり（paste_kind_of）、
    // Ctrl+Shift+V は文字だけ、「HTML のまま貼り付け」は fragment のまま。
    // どれも insert_pasted の 1 本で本文に入る。
    void paste_clipboard();
    void paste_html_fragment();
    void paste_markdown(std::string_view fragment);
    void paste_fragment(std::string_view fragment);
    void paste_text();
    void insert_pasted(std::string_view pasted);
    [[nodiscard]] std::optional<std::string> clipboard_html();
    [[nodiscard]] bool pastes_from_os() const noexcept;
    // 描く選択と Ctrl+C / Ctrl+X が覆う本文は同じ 1 本（ADR 0018 の決定 5）。
    [[nodiscard]] core::OffsetRange highlighted_range() const;
    // 矩形 VISUAL のあいだだけ値を持つ（ADR 0035 の決定 2・8）。描く選択も Ctrl+C / Ctrl+X も
    // 同じ行ごとの範囲を使う。
    [[nodiscard]] std::optional<core::VimBlockRange> block_selection() const;
    // 検索強調の値（ADR 0037 / 0102）。入力中は解析値を所有し、確定後は保持結果を借用する。
    // 選択した値と借用はvisible_linesの同期呼出し内だけ生存する。
    using SearchPattern =
        std::variant<core::VimPattern, std::reference_wrapper<const core::VimPattern>>;
    [[nodiscard]] std::optional<SearchPattern> search_pattern() const;
    [[nodiscard]] static const core::VimPattern &frame_pattern(const core::VimPattern &pattern);
    [[nodiscard]] static const core::VimPattern &
    frame_pattern(std::reference_wrapper<const core::VimPattern> pattern);
    [[nodiscard]] static const core::VimPattern *
    pattern_of(const std::optional<SearchPattern> &pattern);
    [[nodiscard]] LineView line_view(const PaneSource &source, LineView view,
                                     const core::OffsetRange &range, core::Offset caret) const;
    // 枠の見えている行。字句器を持つ言語なら色の区間も付け、cache をその枠の覚えで置き換え、
    // 字句器へ通した行の数を lexed に足す（ADR 0156 の決定 8・9）。覚えの鍵は source.tab。
    [[nodiscard]] std::expected<std::vector<LineView>, TextLayoutFailure>
    visible_lines(const PaneSource &source, std::optional<SyntaxStateCache> &cache,
                  std::size_t &lexed) const;
    [[nodiscard]] std::expected<std::size_t, TextLayoutFailure>
    screen_rows_of(const LineView &shown) const;
    // 格子の見えている画面行（ADR 0140 の決定 5）を frame の grid と grid_scanned に書く。
    // 位置解決と同じ GridCells（本文の索引を借りる）を 1 frame に 1 つ作って切り出す。
    // 失敗は升を読めないとき。構文の色は visible_lines と同じ遡りと覚えで付ける（#839）。
    [[nodiscard]] std::optional<TextLayoutFailure>
    grid_visible_rows(const PaneSource &source, std::optional<SyntaxStateCache> &cache,
                      std::vector<GridRowView> &grid, GridTally &tally) const;
    // もう 1 つの枠の表示値を frame に載せる（ADR 0148 の決定 8）。
    void add_other_pane(EditorFrame &result, const core::VimPattern *pattern);
    // add_other_pane が std::visit で選ぶ 1 対（ADR 0155 の決定 11）。
    void add_other_view(EditorFrame &result, const core::VimPattern *pattern, const PaneView &view);
    void add_other_view(EditorFrame &result, const core::VimPattern *pattern,
                        const PreviewView &view) const;
    void finish_frame(EditorFrame &result, const PaneSource &source, bool gridded,
                      std::size_t lexed);
    // 枠の側の構文の覚え（ADR 0156 の決定 9）。操作中の枠ともう 1 つの枠は別の覚えを持つ。
    [[nodiscard]] std::optional<SyntaxStateCache> &syntax_cache(core::PaneSide side) noexcept;
    // 操作中でない側の覚え。
    [[nodiscard]] std::optional<SyntaxStateCache> &passive_syntax_cache() noexcept;
    // 1 行の塗る範囲。検索の当たりは見えている画面行の本文の範囲 visible に、
    // 一致の長さの上限ぶんの余裕を前後に足した窓だけを読んで照合する（#556）。
    // 今の当たりはキャレット（incsearch の入力中は preview の当たり）を含む当たり。
    [[nodiscard]] GridLineMarks grid_marks(const PaneSource &source, core::LineNumber line,
                                           const core::OffsetRange &visible,
                                           const core::OffsetRange &selection) const;
    // 本文の変換（入力行が開いていれば空）と入力行の変換（入力行が開いているときだけ）。どちらか
    // 一方にだけ載る（ADR 0061 の決定 4・#635 で面から入力行すべてへ）。
    [[nodiscard]] std::optional<CompositionView> composed() const;
    [[nodiscard]] std::optional<CompositionView> command_composed() const;
    // 録画中のマクロの名前。通常モードでは録画が止まっているので出さない（ADR 0046 の決定 8）。
    [[nodiscard]] std::optional<char> recording_name() const;
    [[nodiscard]] core::GuideContext guide_context() const noexcept;
    // Vim の NORMAL では IME を切ってあるので変換は来ないはずだが、来たら捨てる（決定 4）。
    // 入力行（面・Ex の行・検索の行）が開いていれば受ける（ADR 0061 の決定 3・#635）。
    [[nodiscard]] bool composition_ignored() const noexcept;
    // 入力行を閉じる 1 本。閉じるときに残っている変換は消す（ADR 0061 の決定 3）。
    void close_command_input();
    [[nodiscard]] core::FontFamilies given_fonts() const;
    void reach_font_families();
    // 確定した文字列を Vim の打鍵として流す。`.` の再生とマクロが後で自然に載る（決定 4）。
    void type_as_vim_keys(std::string_view utf8);
    void fail(FileFailure failure);
    // 判定済みの列を本文へ渡す。CP932 の変換だけはポートの向こう（ADR 0080）。
    [[nodiscard]] std::expected<core::TextBuffer, FileFailure> decoded(core::DetectedText text);
    [[nodiscard]] std::expected<void, FileFailure> write_document(const SaveDocument &intent);
    // 本文を保存の符号化の断片にする 1 本（ADR 0010・ADR 0137 の決定 4・ADR 0149）。保存と
    // 自動退避が共用する。Shift_JIS と UTF-16 は変換した本文を held に持ち、断片はそれを指す。
    // ending は保存の改行の形で、本文の形と違うときだけ写す（ADR 0010 の追補 #1049 の決定 3）。
    [[nodiscard]] std::expected<std::vector<std::string_view>, FileFailure>
    encoded_parts(const core::TextBuffer &text, core::TextEncoding encoding,
                  core::LineEnding ending, std::string &held);
    [[nodiscard]] std::expected<std::vector<std::string_view>, FileFailure>
    source_parts(std::string source, core::TextEncoding encoding, std::string &held);

    EditorPorts ports_;
    EditorState state_;
    core::TextViewGeometry view_geometry_{};
    // 窓の client の物理画素（ADR 0150 の決定 1）。0 は寸法が届く前。
    std::int32_t window_width_ = 0;
    std::int32_t window_height_ = 0;
    std::optional<TextLayoutFailure> layout_failure_;
    // 今の意図が描き直しを要るか（EditorDelivery.repaint）。apply の入口で true に戻し、合図
    // （AutosaveTick / PreviewTick）が何も変えなかったときだけ false にする（#813）。
    bool repaint_ = true;
    // 頼まれてまだ解決していない追従の位置。後の頼みが前の頼みを置き換える（ADR 0128 の決定 1）。
    std::optional<core::TextPosition> pending_follow_;
    // 行高を変えた設定が頼んだ clamp。新しい幾何が届いた後のスクロールの読みで、追従より先に
    // 1 回だけ解決する（ADR 0128 の決定 8）。
    bool pending_clamp_ = false;
    std::optional<float> wanted_x_;
    // `.` と `@` の再生で流す鍵の列と、それぞれの鍵の入れ子の深さ（ADR 0046 の決定 3・4）。
    // replay_depth_ はいま流している鍵の深さで、再生の外（打った鍵）では空。どれも
    // perform(VimReplay) と queue_nested_replay だけが書く。
    std::deque<core::VimKey> replay_queue_;
    std::deque<std::size_t> replay_depths_;
    std::optional<std::size_t> replay_depth_;
    // 再生を打ち切った鍵の失敗（Issue #983）。perform(VimReplay) が置き、`.` や `@` を打った鍵の
    // deliver_vim_key が自分の失敗として返して消す（Vim の :normal! は残りの鍵を捨てる）。
    std::optional<core::VimRepeatFailure> replay_failure_;
    // 再生の編集を畳む起点（ADR 0046 の決定 3）。再生の外では空。perform(VimReplay) が置いて
    // 消し、再生の中で文書が替わったら reopen_replayed_unit が入った文書の値へ置き直す。
    std::optional<core::EditHistory> replay_history_;
    std::optional<core::TextBuffer> replay_text_;
    // 同じ意図の中で読めなかったタブの数（ADR 0059 の「知らせの文言」）。begin_intent が 0 に戻す。
    std::size_t unreached_tabs_ = 0;
    // 起動の途中（コンストラクタの中）は表示形式の問いを判定しない（ADR 0140 の決定 2）。
    bool starting_ = true;
    // 面が同じフォルダを頼んだ控え（ADR 0062 の決定 14・15）。券は面を開くたびに進み、ほかは
    // 面を開くときに置き直す。面が閉じている間は読まない。
    PaletteFolder palette_folder_;
    // フォルダの券の通し番号（ADR 0162 の決定 5）。プロセスで 1 つで、Ctrl+P
    // の面とエクスプローラーが 頼むたびに進める。
    std::uint64_t folder_tickets_ = 0;
    // FolderPort に最後に頼んで最後の batch がまだ届いていない券。port は新しい要求で前の要求を
    // やめるので、仕事中の券は 1 つだけ。
    std::optional<std::uint64_t> folder_live_;
    // エクスプローラーが頼んで読み終えていないフォルダ（決定 5）。ルートを替えると捨てる。
    std::optional<ExplorerFolder> explorer_folder_;
    // 面の上で押した行の節の経路（決定 4・ADR 0056 の決定 9）。離したときに同じ節かを比べて消す。
    // 「…」の行と行の外を押したときは値なし（離しても何もしない）。
    std::optional<core::FilePath> explorer_pressed_;
    // 字体の一覧（#597・D58）。補完が求めた最初の 1 回だけ読み、読めなくても再読しない。
    std::optional<core::FontFamilies> font_families_;
    // プレビューの券（ADR 0155 の決定 6）。要求ごとに 1 つ進み、閉じて開き直しても戻さない
    // （閉じる前の要求の結果を開き直した後の券と取り違えない）。
    std::uint64_t preview_tickets_ = 0;
    // 読みの port の券（ADR 0159 の決定 5）。プロセスで 1 つの通し番号で、頼むたびに進む。
    std::uint64_t read_tickets_ = 0;
    // 揃ったがまだ採っていない読みの結果（ADR 0159 の追補 2）。歩き・入力行・面の最中は留め、
    // 終わりの意図で頼んだ順に採る。
    std::vector<DocumentReadFinished> arrived_reads_;
    // 起動の復元で見ていたタブが裏の読みになったとき仮に残した無題と、使った順の順位（追補 1）。
    // そのタブへ切り替えたときに無題を外して順位を当てる。
    std::optional<AutosaveId> restore_untitled_;
    // 裏の読みが揃ったときに切り替える先（ADR 0159 の追補 1・6）。最後に SwitchTab・起動の復元・
    // 閉じた後の隣・枠の移りで指されて裏の読みになったタブ。文書が替わると（leave_document）消え、
    // 歩きで飛ばしたタブは入らない。
    std::optional<AutosaveId> reach_wanted_;
    std::vector<std::size_t> restore_ranks_;
    // 前の frame の本文の先頭行（決定 7）。frame だけが読み書きする。
    std::optional<core::LineNumber> preview_body_top_;
    // 自動退避の帳面（ADR 0137 の決定 2）。AutosaveTick の道だけが読み書きする。
    AutosaveLedger autosave_ledger_;
    // 「自動退避できませんでした」を出したか。次に成功するまで 2 回目を出さない（決定 4）。
    bool autosave_notified_ = false;
    // 上限を超えて退避しない知らせを出した文書の退避番号（ADR 0159 の決定 7・L-4）。番号は再利用
    // されないので文書ごとに 1 回。
    std::vector<AutosaveId> autosave_oversized_;
    // 起動の問いの間だけ持つ残骸の列（決定 6）。autosave_remnants() が置き、答えの意図が空にする。
    std::vector<AutosaveRemnant> autosave_remnants_;
    // 枠の側ごとの構文の状態の覚え（ADR 0156 の決定 9・first と second の 2 つ）。frame だけが
    // 読み書きする。
    std::optional<SyntaxStateCache> first_syntax_;
    std::optional<SyntaxStateCache> second_syntax_;
    // 言語サーバーとのつながり（ADR 0160 の決定 5・6）。EditorControllerLanguageServer.cpp の群
    // だけが読み書きする。
    LanguageServerLink language_server_;
    // Vim の `map` の表（ADR 0176 の決定 2）。設定から refresh_key_map だけが作る。
    core::KeyMap key_map_;
    // この意図が頼む待ちのタイマー（EditorDelivery.map_timer）。apply の入口で空に戻し、待ちを
    // 積む・流す所だけが置く。
    std::optional<MapTimer> map_timer_;
};
} // namespace nenenib::application
