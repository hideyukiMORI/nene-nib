#pragma once

#include "AppearancePort.hpp"
#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveStep.hpp"
#include "CaretMotion.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "CommandChoice.hpp"
#include "CommandEdit.hpp"
#include "CommandPalette.hpp"
#include "CompletionDirection.hpp"
#include "CompletionEntry.hpp"
#include "CompletionList.hpp"
#include "CompletionSession.hpp"
#include "CompletionView.hpp"
#include "CompositionView.hpp"
#include "ContextMenuItem.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuOpen.hpp"
#include "ContextMenuView.hpp"
#include "DetectedText.hpp"
#include "Diagnostic.hpp"
#include "DiagnosticCommand.hpp"
#include "DiagnosticsCount.hpp"
#include "DisplayLanguage.hpp"
#include "Distribution.hpp"
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
#include "EditHistory.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "ExClipboardRequest.hpp"
#include "ExDisplayLanguageRequest.hpp"
#include "ExDocumentRequest.hpp"
#include "ExFormatRequest.hpp"
#include "ExLanguageRequest.hpp"
#include "ExLineRequest.hpp"
#include "ExLspRequest.hpp"
#include "ExMapRequest.hpp"
#include "ExPaletteRequest.hpp"
#include "ExPreviewRequest.hpp"
#include "ExPrintRequest.hpp"
#include "ExRelativeLineRequest.hpp"
#include "ExResult.hpp"
#include "ExRootRequest.hpp"
#include "ExSyntaxRequest.hpp"
#include "ExTabRequest.hpp"
#include "ExWindowCommand.hpp"
#include "ExplorerEntry.hpp"
#include "ExplorerFolder.hpp"
#include "ExplorerRow.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerView.hpp"
#include "ExplorerWidth.hpp"
#include "FileBookmarks.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "FindPhase.hpp"
#include "FinderPreview.hpp"
#include "FinderPreviews.hpp"
#include "FolderBatch.hpp"
#include "FontFamilies.hpp"
#include "FontSize.hpp"
#include "FoundLanguageServer.hpp"
#include "GridLineMarks.hpp"
#include "GridRowView.hpp"
#include "GridTally.hpp"
#include "ImageOutcome.hpp"
#include "IntentMark.hpp"
#include "JsonValue.hpp"
#include "KeyMap.hpp"
#include "LandingPlace.hpp"
#include "Language.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageRootGap.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerRowState.hpp"
#include "LanguageServerRowView.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageServerTrouble.hpp"
#include "LanguageServers.hpp"
#include "LanguageSymbols.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "LinkCardView.hpp"
#include "LspCompletionList.hpp"
#include "LspLanding.hpp"
#include "LspLocation.hpp"
#include "LspNotification.hpp"
#include "LspPosition.hpp"
#include "LspProgressValue.hpp"
#include "LspRequest.hpp"
#include "LspRequestContext.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "MapTimer.hpp"
#include "MinimapCache.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenFailureReply.hpp"
#include "OpenLanguageServerLog.hpp"
#include "OpenRequest.hpp"
#include "OpenedAs.hpp"
#include "OpenedDocument.hpp"
#include "PaletteDiagnostic.hpp"
#include "PaletteFolder.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteProjectSearch.hpp"
#include "PaletteReference.hpp"
#include "PaneSide.hpp"
#include "PaneSizing.hpp"
#include "PaneSource.hpp"
#include "PaneSplit.hpp"
#include "PendingLspContext.hpp"
#include "PendingLspRequest.hpp"
#include "PointerClicks.hpp"
#include "PreviewAnchor.hpp"
#include "PreviewFullRoute.hpp"
#include "PreviewKey.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewRequest.hpp"
#include "PreviewSnippet.hpp"
#include "ProjectHitLanding.hpp"
#include "ProjectHitRow.hpp"
#include "ProjectSearchBatch.hpp"
#include "ProjectSearchView.hpp"
#include "ReplayLevel.hpp"
#include "ReplayRounds.hpp"
#include "ResizePane.hpp"
#include "ResizeSplit.hpp"
#include "ScrollState.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerErrorOutput.hpp"
#include "ServerEvent.hpp"
#include "ServerExited.hpp"
#include "ServerFailed.hpp"
#include "ServerLaunch.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "Session.hpp"
#include "SettingsOpen.hpp"
#include "SettingsSection.hpp"
#include "SettingsView.hpp"
#include "SignatureTip.hpp"
#include "SplitExtent.hpp"
#include "SplitState.hpp"
#include "StartEntry.hpp"
#include "StartLists.hpp"
#include "StartScreenView.hpp"
#include "SubstituteQuestion.hpp"
#include "SubstituteReply.hpp"
#include "SyntaxStateCache.hpp"
#include "TabNamePart.hpp"
#include "TabReach.hpp"
#include "TabTipTimer.hpp"
#include "TextBuffer.hpp"
#include "TextCaret.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TitleBarInput.hpp"
#include "UiMessage.hpp"
#include "UnloadedDocument.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimChange.hpp"
#include "VimCharacter.hpp"
#include "VimConfirmSubstitute.hpp"
#include "VimDiagnosticCommand.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimLineMarks.hpp"
#include "VimMapMode.hpp"
#include "VimMarks.hpp"
#include "VimPattern.hpp"
#include "VimRecordedKeys.hpp"
#include "VimRegister.hpp"
#include "VimRepeatFailure.hpp"
#include "VimReplay.hpp"
#include "VimReplayKind.hpp"
#include "VimRowSpan.hpp"
#include "VimSearchDirection.hpp"
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
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
// ミニマップが最後に送った位置（先頭の行と、その行の中の画面行・ADR 0171 の決定 10 の追補）。
using MinimapAt = std::pair<core::LineNumber, std::size_t>;

// 状態遷移はここだけ（ARC-004 / ARC-011）。UI は apply の返す EditorDelivery を写す。
// 意図は閉じた和型で、std::visit の写し先が足りなければコンパイルが落ちる（CPP-002 / ADR 0009）。
class EditorController final
{
  public:
    // 起動引数のファイルは順にタブで開く（ADR 0056 の決定 13）。どれも通常の OpenDocument を通る。
    // distribution は製品の形で、合成ルートが起動時に 1 回 OS に問うた値（ADR 0178 の追補 #1809）。
    EditorController(EditorPorts ports, core::Distribution distribution,
                     const std::vector<OpenDocument> &initial = {});
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
    // 面の配置へ渡す置き方（ADR 0189 の決定 1・2）。面が閉じている・欄を持たない出どころは off。
    [[nodiscard]] core::FinderPreview palette_placement() const noexcept;
    // 面の表示値にプレビュー欄の中身を載せるか（決定 1・レビュー V-1792 の 2）。置き方が off か、
    // 寸法が届いた窓で入らずに off で描くなら偽（面の打鍵の道で描かない欄の窓を作らない）。
    [[nodiscard]] bool preview_drawn(core::FinderPreview placement) const noexcept;
    // 試験の harness が Vim の鍵を 1 つ打つ口（ADR 0048 の決定 8）。窓の VimKeyPress と違い、
    // 入力行が開いていれば再生と同じ command_key の写しで入力行へ入る。
    [[nodiscard]] EditorFrame press_vim_key(const core::VimKey &key);
    // fixture の harness が :normal! の意味で流す口（Issue #230）。鍵を順に press_vim_key と
    // 同じに流し、鍵が閉じた失敗で終わったら残りの鍵を流さない。窓で打った鍵は失敗しても
    // 次の鍵が走るので、この口は通らない。各鍵の後に追従を解決する（Vim の
    // update_topline・#1154）。失敗で打ち切った所が INSERT なら :normal! と同じく Esc で終える
    // （#1246）。
    [[nodiscard]] EditorFrame press_vim_keys(std::span<const core::VimKey> keys);
    [[nodiscard]] EditorFrame frame();
    [[nodiscard]] bool command_line_active() const noexcept;
    // 通常モードの「検索: 」が開いているか（F3 / Shift+F3 を当たりへの移動にする・ADR 0174 の
    // 決定 6）。
    [[nodiscard]] bool find_line_active() const noexcept;
    [[nodiscard]] bool command_palette_active() const noexcept;
    // 本文の右クリックのメニューが開いているか（ADR 0078 の追補 #1051）。窓が鍵とマウスの行き先を
    // 決めるための安い読みで、frame を作らない。
    [[nodiscard]] bool context_menu_active() const noexcept;
    // プレビューのリンクの札が出ているか（施主決定 D106・#1869 の段 L2）。窓が Enter・Esc・
    // Ctrl+C と押しの行き先を決めるための安い読みで、frame を作らない（frame の link_card と同じ
    // 判定）。
    [[nodiscard]] bool link_card_active() const;
    // 設定の面が開いているか（ADR 0170 の 2026-11-18 の追補 2）。窓が鍵の行き先を決めるための安い
    // 読みで、frame を作らない（鍵の振り分けは ui の工程）。
    [[nodiscard]] bool settings_open() const noexcept;
    // 設定の面の表示値（開いていなければ値なし）。frame に載る値と同じで、窓が鍵とマウスの後に
    // 焦点の行と送りを読む口（本文の frame を作らない・#1836）。
    [[nodiscard]] std::optional<SettingsView> settings_view() const;
    // 開いているメニューの選んでいる行（マウスの動きのたびに窓が比べる安い読み）。
    [[nodiscard]] std::optional<std::size_t> context_menu_highlighted() const noexcept;
    // 補完の面が見えているか（ADR 0190 の決定 5）。窓が上下・Enter・Tab・Esc の行き先を決めるための
    // 安い読みで、frame を作らない。completion_open は面の状態があるか（LSP の答えを待っていて
    // まだ見えていない面を含む・窓が非アクティブになったときに閉じるかを決める）。
    [[nodiscard]] bool completion_active() const noexcept;
    [[nodiscard]] bool completion_open() const noexcept;
    // 選んでいる候補の確定が本文を変えるか（面が見えていなければ偽・#1844 の L1）。窓が Enter と
    // Tab を 面へ送るかを決める安い読みで、偽なら窓は面を閉じて鍵を本文へ通す。
    [[nodiscard]] bool completion_commits() const;
    // 帯のタブの退避番号の並び（帯の順）とアクティブなタブの位置（ADR 0078 の追補 #1790）。窓が
    // 「ほかのタブを閉じる」「右のタブを閉じる」で次に閉じるタブを引き直す安い読みで、frame を
    // 作らない。
    [[nodiscard]] const std::vector<AutosaveId> &tab_ids() const noexcept;
    [[nodiscard]] std::size_t active_tab() const noexcept;
    // エクスプローラーの面に焦点があるか（ADR 0162 の決定 4・E4 で足した口）。窓が鍵の行き先を
    // 決めるための安い読みで、frame を作らない。面が消えていれば偽。
    [[nodiscard]] bool explorer_focused() const noexcept;
    // 面の表示値（決定 3・8）。窓が押した行の木の位置（first_visible を足す）を決めるための口で、
    // frame の本文の行は作らない（command_palette_view と同じ形）。面が消えていれば値なし。
    [[nodiscard]] std::optional<ExplorerView> explorer_view() const;
    // 起動画面の表示値（ADR 0172 の決定 4・5）。窓が押した行と離した行を決める口で、
    // frame の本文の行は作らない（explorer_view と同じ形・押しと離しのときだけ呼ぶ）。
    // 出ていなければ値なし。
    [[nodiscard]] std::optional<StartScreenView> start_screen() const;
    // 出ている面の幅の求め（ADR 0162 の追補 D78）。ドラッグの間はその幅、ほかは設定
    // explorer_width。
    // 面が消えていれば値なし。窓が境の当たり・面と本文の配置を引く安い読みで、frame を作らない
    // （マウスの動くたびに呼ばれる）。frame の ExplorerView.width と同じ値。
    [[nodiscard]] std::optional<core::ExplorerWidth> explorer_width() const noexcept;
    // マウスを載せた面の画面の行（HoverExplorer.line と同じ値・#1390）。窓がマウスの動くたびに
    // 「変わったか」を見る安い読みで、frame を作らない（title_bar_input の hovered と同じ形）。
    [[nodiscard]] std::optional<std::size_t> explorer_hovered() const noexcept;
    // マウスを載せたプレビューの表の横の送りの棒の表の通し番号（HoverPreviewTable.table と同じ値・
    // #1883）。窓がマウスの動くたびに「変わったか」を見る安い読みで、frame を作らない。
    [[nodiscard]] std::optional<std::size_t> preview_table_hovered() const noexcept;
    // 帯の配置の入力を状態の 4 つの値（タブの本数・アクティブの位置・送り量・マウスを載せた要素）
    // から読むだけの口。行も題名も作らない（ADR 0056 の決定 9）。窓の hit test と hover が
    // マウスの動くたびに呼ぶ。width は帯の幅（物理画素）。値は title_bar_input(frame(), ...)
    // と同じ。
    [[nodiscard]] core::TitleBarInput title_bar_input(std::int32_t width, std::uint32_t dpi) const;
    // アクティブな文書の診断の件数（ADR 0161 の決定 4）。数えるのは core::diagnostic_counts の
    // 1 本で、frame の diagnostic_counts と窓のステータスバーの hit test（status_bar_layout）が
    // 同じ値を引く。frame を作らない。
    [[nodiscard]] core::DiagnosticCounts diagnostic_counts() const;
    // 知らせの欄にキャレットの上の診断の文が出ているか（frame の diagnostic_message の有無と同じ
    // 値・ADR 0161 の決定 4 の追補）。窓がステータスバーのクリックを決める口で、frame を作らない。
    [[nodiscard]] bool diagnostic_message_shown() const;
    // アクティブな文書を受ける言語サーバーの席の 1 行（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。
    // `initialize` の答えを待つ間は「<言語>: 起動中」、running で `$/progress` の動いている
    // token があれば最後に動いたものの「<言語>: <題> <文> <百分率>%」、ほかは値なし。frame の
    // language_server_status が同じ値を引く。lsp がオフで子が居なければ欄を 1 つ読むだけで値なし。
    // frame を作らない。
    [[nodiscard]] std::optional<core::DisplayText> language_server_status() const;
    // アクティブな文書を言語サーバーの席が受けているか（起こしている・`initialize` を待つ・
    // running・#1950 のレビュー R3）。frame の language_server_served と窓のステータスバーの
    // hit test（status_server_item）が同じ値を引き、真の間は右端の欄の幅を変えない。lsp がオフで
    // 子が居なければ欄を 1 つ読むだけで偽。frame を作らない。
    [[nodiscard]] bool language_server_served() const;
    // 無名レジスタと Vim のモードは表示値に載らないので、fixture の再生だけがここを読む
    // （ADR 0012 の決定 7）。状態を変える口はここには無い。
    [[nodiscard]] const core::VimState &vim_state() const noexcept;
    // INSERT の Ctrl-n / Ctrl-p の候補の列（ADR 0169 の決定 1・2）。アクティブな文書の語の表が
    // 無ければ（1 MiB 以下なら）ここで 1 回作って状態に置き（表を作るのはこの 1 か所・設計席の追補
    // 2026-11-18）、キャレットで insert_completion_list を呼ぶ。面と鍵（#1008 の段 B）が呼ぶ口。
    [[nodiscard]] core::CompletionList insert_completion(core::CompletionDirection direction);
    // アクティブな文書の語の表（作っていなければ空）。性質の試験が「状態の表＝本文から作り直した
    // 表」を確かめるための読む口で、状態を変えない。
    [[nodiscard]] const std::optional<core::WordTable> &word_table() const noexcept;
    // `@` の再生の中で `map` の左辺の途中まで読んで待った回数（起動からの累計・#1312）。性質の
    // 試験が待ちと flush の道を通ったことを確かめるための口で、状態を変えない。
    [[nodiscard]] std::size_t replay_waits() const noexcept;
    // 再生の列へ積んだ周の数（起動からの累計・#1341・`.` の再生も 1 周に数える）。`[count]@a` は
    // 1 周ずつ積むので、巨大な回数でも鍵が失敗した所で止まったことを、時計を読まずに数で確かめる
    // ための口。
    [[nodiscard]] std::size_t replay_rounds() const noexcept;
    // 再生の鍵の数の上限を置き換える試験の口（#1351）。製品は呼ばない（既定は
    // core::vim_replay_key_limit）。Debug で 100,000 鍵を流すと 1 件 80 秒かかるので、契約は小さい
    // 上限で境を確かめる。知らせの文は製品の値のまま変えない。
    void limit_replay_keys(std::size_t limit) noexcept;
    // Ctrl+Tab で歩いている間だけ真（ADR 0058 の決定 4）。窓が Ctrl を離したとき・フォーカスを
    // 失ったときに SettleRecentTab を送るかを決めるための口で、状態の 1 欄を読むだけ。
    [[nodiscard]] bool tab_walking() const noexcept;
    // 枠の配置と操作中の枠の側（ADR 0148 の決定 8）。窓がマウスの点の下の枠と、本文の行数を
    // 数える枠（pane_layouts の片側）を引くための口で、状態の 1 欄を読むだけ。
    [[nodiscard]] const SplitState &split() const noexcept;
    // 配置と境界の比を畳んだ値（pane_layouts の引数・ADR 0148 の 2026-11-22 の追補）。状態の 2 欄を
    // 読むだけ。
    [[nodiscard]] core::PaneSplit pane_split() const noexcept;
    // 保存した字の大きさ（入力行の評価も字の大きさは重ねない・shown_settings と同じ値）。窓がマウス
    // の動くたびに本文の枠の配置を引く口で、delivery も frame も作らない。
    [[nodiscard]] core::FontSize font_size() const noexcept;
    // プレビューの見せ方（ADR 0155 の改訂 15）。窓が tick
    // のタイマー・クリック・ホイールを決める口で、 状態を読むだけ。
    [[nodiscard]] PreviewPlacement preview_placement() const noexcept;
    // ミニマップを出すか（ADR 0171 の決定 1）。窓が本文の枠の幅を数えるのと、出した・消したときに
    // 寸法を送り直すのに読む口。設定（sync_minimap が置いた 1 欄）がオンで、全面プレビューでない
    // とき（本文が隠れている間は面を出さない・レビュー L6）。frame の minimap と同じ条件。
    [[nodiscard]] bool minimap_shown() const noexcept;
    // 解決済みの表示言語（ADR 0123 の追補 2026-11-13 の決定 3・#1019）。ui が自分で出す文
    // （MessageBox・ShutdownBlockReason・ファイルの選択）を引く口で、frame を作らずに読む。
    [[nodiscard]] core::DisplayLanguage display_language() const noexcept
    {
        return state_.display_language();
    }
    // アクティブな文書の本文を読むだけの口。試験が仮想桁の索引の持ち方（ADR 0140 の決定 4）を
    // 確かめるために読む。状態を変える口はここには無い。
    [[nodiscard]] const core::TextBuffer &text() const noexcept;
    // アクティブな文書の undo の履歴とマークを読むだけの口。fixture の再生が Vim の変更の印
    // （ADR 0181）を比べるために読む。
    [[nodiscard]] const core::EditHistory &history() const noexcept;
    [[nodiscard]] const core::VimMarks &vim_marks() const noexcept;
    // 前回のプロセスが残した退避（ADR 0137 の決定 6）。ui が窓を見せて最初の frame を描いた後に
    // 1 回だけ呼ぶ。port の remnants() を 1 回呼んで列を覚え、その列を返す（失敗は空）。
    // RestoreAutosave と DiscardAutosave はこの覚えた列を使い、port へもう 1 回聞かない。
    [[nodiscard]] const std::vector<AutosaveRemnant> &autosave_remnants();
    // 最後の砦（ADR 0137 の追補 #1625）。捕まえていない例外で落ちる直前に、UI スレッドの
    // std::terminate の手続きだけが 1 回呼ぶ。決定 9 と同じ退避（変わったタブの store と flush）を
    // last_resort_plan の計画で回す（意図の途中で動いたタブは書かず前の退避を残す）。一覧・履歴・
    // 読みの取消・言語サーバーには触れない（accept(EndSession) を通さない）。頼んだ退避が全部
    // 書けたら真（上限を超えて書かない文書・flush の時間切れ・書けなかった結果があれば偽）。
    [[nodiscard]] bool back_up_before_defect();
    // 言語サーバーへの文脈つきの要求（ADR 0190 の決定 1〜3）。段 B / C の意図（補完の面・`K`・
    // `gd`・F12）が呼ぶ口で、それまでは試験が直に呼ぶ。本体は
    // EditorControllerLanguageRequests.cpp。サーバーが running で、その能力を持ち、アクティブな
    // 文書を `didOpen` してあるときだけ、未送信の変更を流してから送り、同じ種類の古い要求は
    // `$/cancelRequest` で取り消す。送れたら真。
    // session は補完の面の通し番号、trigger はその要求を起こした `triggerCharacters` の 1 つ（手で
    // 開いたなら値なし）。
    [[nodiscard]] bool request_completion(std::uint64_t session,
                                          std::optional<std::string_view> trigger = std::nullopt);
    [[nodiscard]] bool request_hover();
    [[nodiscard]] bool request_definition();
    // 参照（#1947・ADR 0191 の決定 4）。`includeDeclaration: true`。
    [[nodiscard]] bool request_references();
    // 記号（#1955・ADR 0191 の決定 5）。今の文書の記号の一覧。
    [[nodiscard]] bool request_document_symbols();
    // 引数の情報（#1959・ADR 0191 の決定 6）。trigger は打った `triggerCharacters` の 1 文字
    // （手で頼んだら値なし）。
    [[nodiscard]] bool request_signature_help(std::optional<std::string> trigger, bool retrigger);
    // 整形（#1993・ADR 0191 の決定 7）。selection が真なら選択範囲（rangeFormatting）。
    [[nodiscard]] bool request_formatting(bool selection);
    // 文脈の照合を通った最新の答え（決定 1）。状態の 1 欄を読むだけ。
    [[nodiscard]] const LanguageAnswers &language_answers() const noexcept;
    // Ctrl+P の一覧のために参照と記号の当たりを作った数（#1958 のレビュー R1 の契約が読む）。
    [[nodiscard]] std::size_t listed_hits_made() const noexcept;
    // アクティブな文書を受ける席の、`initialize` の答えから読んだサーバーの能力（決定 3）。段 B の
    // `triggerCharacters` の照合と試験が読む口で、受ける席が無ければ空。
    [[nodiscard]] const ServerAbilities &language_server_abilities() const
    {
        return active_language_server().abilities;
    }

  private:
    // 意図の前に 1 意図ぶんだけの表示（失敗・報せ）を消す。keeps_message は報せを残す意図。
    void begin_intent(bool keeps_message);
    // 意図の出口（ADR 0179 の決定 4-1）。プレビューのタブが保存済みでなくなっていれば固定する。
    void settle_transient();
    void forget_unkept_marks(const EditorIntent &intent);
    // エクスプローラーのダブルクリックの 2 打目（DBLCLK と続く離し）は 1 打目の知らせを消さない
    // （レビュー V-1391b の R-b・#1390・EditorControllerExplorer.cpp）。
    [[nodiscard]] bool keeps_explorer_notice(const EditorIntent &intent) const noexcept;
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
    // deliver_vim_key。再生（`.`・マクロ・press_vim_keys）と IME の確定はここを通らない（マクロの
    // 再生は replayed_macro_key が同じ key_map_step で引く・#1312）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> typed_vim_key(const core::VimKey &key);
    // key_map_step の答えごとの写し（std::visit・答えが増えたらコンパイルが落ちる）。key は今の
    // 鍵。Emit は積んだ鍵と今の鍵の列を作って delivered_units に渡す（引く前に待ちがあれば、
    // 流して空にしたらタイマーを外す）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> typed_step(core::KeyMapPass pass,
                                                                   const core::VimKey &key);
    [[nodiscard]] std::optional<core::VimRepeatFailure> typed_step(core::KeyMapPending pending,
                                                                   const core::VimKey &key);
    [[nodiscard]] std::optional<core::VimRepeatFailure> typed_step(core::KeyMapEmit emitted,
                                                                   const core::VimKey &key);
    // 表が決めた鍵の列を単位（右辺 1 つか表に無い 1 鍵）ごとに deliver_vim_key へ流す。単位の
    // 途中の鍵が失敗したらその単位の残りを捨てる（Vim の beep_flush の FLUSH_MINIMAL・#1257）。
    // 単位を流した後に表を引くモードが変わっていたら、typed の読んでいない残り（後の単位と待ち）
    // を今のモードで引き直す（Vim の vgetorpeek は鍵ごとに今の State で表を引く・#1266）。
    // 返すのは最後に流した鍵の答え。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    delivered_units(const core::KeyMapEmit &emitted, std::span<const core::VimKey> typed);
    // 1 単位を流す。plain（表に無い鍵・`noremap` の右辺・左辺で始まる右辺のその分）はそのまま、
    // remapped（`map` の右辺の残り）は 1 鍵ごとに流す時点のモードで rhs_unit_step に 1 段だけ
    // 引かせて流す（#1276）。途中の鍵が失敗したら残りを捨て、その答えを返す。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    delivered_unit(std::span<const core::VimKey> plain, std::span<const core::VimKey> remapped);
    // 打った鍵の入口の 1 単位。表に当たった単位は右辺の鍵として（engine はマクロに積まない）流し、
    // 録画が単位の前に立っていれば打った左辺 lhs を右辺の最初の鍵の後で録画に積む。最初の鍵が
    // 録画を止めたら左辺も録らない（Vim の get_recorded が消すのは `q` を返した vgetc の間に録った
    // 鍵だけ・`nmap Z jq` は Z を残す・ADR 0176 の追補 32・33・#1312）。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    typed_unit(bool mapped, std::span<const core::VimKey> lhs, std::span<const core::VimKey> plain,
               std::span<const core::VimKey> remapped);
    // keys を順に deliver_vim_key へ流し、失敗した鍵で止めてその答えを返す。鍵を 1 つ流したら
    // 置いてある左辺を recorded_held_lhs で積む。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    delivered_keys(std::span<const core::VimKey> keys);
    // typed_unit が置いた左辺を、まだ録画が立っていれば録画に積んで消す（追補 32・#1312）。
    void recorded_held_lhs();
    // 読んでいない鍵の列 rest を引き直す。表を引けないモードの間は 1 鍵ずつそのまま流し、引ける
    // モードになったら最後の 1 鍵の前までを待ちに置いて打った鍵の入口で引く。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    typed_again(std::span<const core::VimKey> rest);
    // 次に読む鍵 front から表を引くモード。Vim モードで入力行が閉じていて、1 鍵の引数を待たず、
    // front が回数の続きの `0` でないときだけ値がある（core::vim_map_mode_of・#1311）。
    [[nodiscard]] std::optional<core::VimMapMode>
    key_map_mode(const core::VimKey &front) const noexcept;
    // 積んだ鍵を key_map_flush で流して空にする（MapTimeout と、待ちを保たない意図の入口）。
    void flush_key_map_pending();
    // 積んだ鍵を流さずに捨てる（窓が閉じていく EndSession だけ）。
    void discard_key_map_pending();
    // 入口で、意図を写す前に待ちの鍵を流すか捨て、Vim の先頭の判定を捨てる（#1374）。
    void settle_pending_keys(const EditorIntent &intent);
    [[nodiscard]] EditorDelivery timed(EditorDelivery delivered) const;
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
    // 入力行の鍵の `<Up>` `<Down>`。履歴を動けなければ not_moved（Vim の beep_flush が先読みを
    // 捨てる・ex_getln.c・#1528）で、再生と :normal! の残りを打ち切る。
    [[nodiscard]] std::optional<core::VimRepeatFailure> recalled_key(core::CommandEdit direction);
    // 入力行の種類で読み替える編集（#1550・#1643）。Vim の行でない入力の Ctrl+U / Ctrl+W は捨て、
    // Vim の `/` `?` の Tab は文字の Tab として入れる。読み替えたら真。
    [[nodiscard]] bool rerouted_edit(const EditCommand &intent);
    // 1 鍵を engine へ流して効果を写す唯一の経路（ADR 0030 の決定 7）。打った鍵も再生の鍵も
    // ここを通り、鍵が閉じた失敗で終わったら理由を返す（ADR 0046 の決定 3）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> step_vim(const core::VimKey &key);
    // engine の 1 歩（鍵の vim_step か `:N` の vim_line_jumped）を測って写す 1 本。
    // 要求の往復・マーク・効果・キャレットの寄せは鍵と同じ（Issue #847）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> stepped_vim(const VimStepSource &source);
    // INSERT の `Ctrl-o` の始め直し（ADR 0182 の決定 2・8）。engine を呼ぶのは restart_vim_insert
    // の 1 本で、鍵の後（due）と engine の外の命令の後（入力行・クリック）の 2
    // つの合図がそこを通る。
    void restart_due_vim_insert(core::Offset command_caret);
    void resume_vim_insert();
    void restart_vim_insert(core::Offset command_caret);
    // stepped_vim が測った 1 歩を状態へ写す（寄せと先頭の判定の前まで）。
    void applied_vim_step(const core::VimStep &step, core::VimMode before);
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
    // 再生の最初の周を列の先頭へ差し込み、残りの周を replay_rounds_ に置く（#1341）。深さが
    // 上限を超えたら何も差し込まず残りを捨てる（決定 4）。
    void queue_replay(const core::VimReplay &effect, std::size_t depth);
    // 1 周ぶんの鍵を列の先頭へ差し込む。more なら最後の鍵に round_end を立てる。
    void queued_round(std::span<const core::VimKey> keys, ReplayLevel level, bool more);
    // round_end の鍵を取り出したとき、replay_rounds_ の頂の次の周を積む。
    void next_replay_round();
    // 鍵の失敗と深さの上限で、列・周・待ちを全部捨てる。
    void dropped_replay();
    // 再生の列から次の 1 歩を流してよいか（#1351・施主決定 D75）。いちばん外の再生で流した歩が
    // core::vim_replay_key_limit に達していたら、列・周・待ちを捨てて 1 行知らせ、偽を返す。
    [[nodiscard]] bool replay_step_allowed();
    // 再生の列を先頭から流す 2 本（#1312）。表が空なら plain（今までの道のまま・鍵ごとの分岐と
    // 割り当てを足さない）、表に行があれば mapped（`@` の鍵は流す時点の表で引く・`.` の鍵は
    // 引かない）。どちらも失敗した鍵で列の残りを捨てる。
    void replayed_plain_queue();
    void replayed_mapped_queue();
    // mapped の列の先頭の 1 鍵。macro の鍵は replayed_macro_key、repeat の鍵はそのまま流す
    // （待ちがあれば先に流す）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> replayed_front();
    // `@` の 1 鍵を打った鍵の入口と同じ key_map_step で引く。待ちは再生の中だけで持つ
    // replay_pending_ に積み、state_ の待ち（打った鍵の分）とは混ぜない。
    [[nodiscard]] std::optional<core::VimRepeatFailure> replayed_macro_key(const core::VimKey &key);
    [[nodiscard]] std::optional<core::VimRepeatFailure> replayed_step(core::KeyMapPass pass,
                                                                      const core::VimKey &key);
    [[nodiscard]] std::optional<core::VimRepeatFailure> replayed_step(core::KeyMapPending pending,
                                                                      const core::VimKey &key);
    [[nodiscard]] std::optional<core::VimRepeatFailure> replayed_step(core::KeyMapEmit emitted,
                                                                      const core::VimKey &key);
    // 引いた答えの最初の単位だけを流す。読んでいない残り（後の単位と待ち）は先に列の先頭へ戻す
    // ので、単位の中の `@` の鍵はその前に入り（Vim は typebuf の先頭へ差し込む）、残りは単位の
    // 後のモードで引き直される（追補 23 の写し）。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    replayed_unit(const core::KeyMapEmit &emitted, std::span<const core::VimKey> typed);
    // 列が尽きたときと `.` の鍵の前に、再生の待ちを key_map_flush で流す（タイマーは使わない）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> flushed_replay();
    // keys を今の深さと kind で再生の列の先頭へ戻す。
    void requeued(std::span<const core::VimKey> keys, core::VimReplayKind kind);
    void accept(const RefreshAppearance &);
    // 開く道の 1 本（ADR 0056 の決定 5・ADR 0060 の決定 7・ADR 0159 の決定
    // 1・3〜5）。同じファイルの タブへ切り替える → 読みの port に頼む。同期に揃えば adopt_read
    // で続け、裏へ回れば読み込み中の 要求を置いて戻る。同じファイルを読み込み中なら 2
    // 本目を頼まない。ほかの読み込み中の要求は 取り消す。開けなければタブを足さず、reply
    // の告げ方で告げる。開けたら opened_as の開き方で開く（ADR 0179 の決定 3）。
    void open_document(const core::FilePath &path, OpenFailureReply reply, OpenedAs opened_as);
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
    void finish_reading(const core::FilePath &path, OpenRequest request,
                        std::expected<OpenedDocument, FileFailure> document);
    void finish_reading(const core::FilePath &path, AutosaveId tab,
                        std::expected<OpenedDocument, FileFailure> document);
    void finish_tab_read(AutosaveId tab, std::expected<OpenedDocument, FileFailure> document);
    [[nodiscard]] std::optional<std::size_t> tab_position_of(AutosaveId tab) const;
    // 揃った文書を開く（決定 4）。待っている間に同じファイルのタブができていればそこへ切り替え、
    // 何も書いていない無題ならそこに開き、ほかは新しいタブを足す。transient で開くときは、
    // プレビューのタブがあれば置き換え、無ければ開いたタブに印を付ける（ADR 0179 の決定 3）。
    void open_read(const core::FilePath &path, OpenedDocument opened, OpenedAs opened_as);
    // 開いた本文を何も書いていない無題に置くか、アクティブの右に新しいタブを足して置く（決定 5 の
    // (b)(c)）。open_read と言語サーバーの記録の写しが同じ 1 本を通る。
    void place_opened(core::TextBuffer text, Document document);
    // 既に開いているタブ position へ切り替える（決定 3-1）。pinned ならそのタブの印を消す
    // （決定 3 の pinned・4-3・4-5）。transient なら印は変えない。
    void reach_open_tab(std::size_t position, OpenedAs opened_as);
    // 同じファイルを読み込み中の開く要求があれば、pinned の要求でその開き方を pinned に改める
    // （揃ったときにプレビューのタブにしない・決定 4-3）。あれば真。
    [[nodiscard]] bool joined_reading(const core::FilePath &path, OpenedAs opened_as);
    // path を読み込み中の開く要求があるか。
    [[nodiscard]] bool opening(const core::FilePath &path) const;
    // 開いたアクティブなタブに、transient なら印を付ける（決定 3-3）。
    void mark_opened(OpenedAs opened_as);
    // アクティブなタブがプレビューのタブなら印を消す（決定 4-2 の保存の成功）。
    void pin_active_tab();
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
    // opened_as はエクスプローラーの面のクリック 1 回だけが transient（ADR 0179 の決定 3）。
    void open_listed(const core::FilePath &path, std::optional<core::PaletteOrigin> origin,
                     OpenedAs opened_as);
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
    // プロジェクトの全文検索（ADR 0167 の決定 1・Ctrl+Shift+F）。`:grep` と同じ口で入力 `!` の面を
    // 開く。入力行や面が開いていれば、取り消してから置き換える。
    void accept(const OpenProjectSearch &);
    // プレビュー欄の置き方を次へ（ADR 0189 の決定 3・EditorControllerFinderPreview.cpp）。設定の
    // 1 本 persist_settings で書く（変わったときだけ）。
    void accept(const CycleFinderPreview &);
    // ---- 探して見る面のプレビュー欄（ADR 0189・EditorControllerFinderPreview.cpp）
    // 面で選んでいる候補の表示値（決定 4）。欄を持たない出どころ・候補が無いときは値なし。段 P1 は
    // 開いているタブの文書だけ手元の本文から窓を作り、ほかは見出しと 1 行の知らせ。
    [[nodiscard]] std::optional<PreviewSnippet>
    preview_snippet(const core::CommandPalette &palette) const;
    // 帯の位置 position の文書の本文と言語（アクティブ・読み込み済みの脇の束）。まだ読んでいない
    // 文書と範囲の外は値なしで、読まない（reach_tab を呼ばない・決定 4）。
    [[nodiscard]] std::optional<std::pair<const core::TextBuffer *, core::Language>>
    tab_text_at(std::size_t position) const;
    // 候補が指すファイル（パスの無い候補は無し）と、それを開いているタブの帯の位置。
    [[nodiscard]] std::pair<std::optional<core::FilePath>, std::optional<std::size_t>>
    preview_target(const core::CommandChoice &choice) const;
    [[nodiscard]] std::pair<std::optional<core::FilePath>, std::optional<std::size_t>>
    diagnostic_preview_target(const core::CommandChoice &choice) const;
    // 全文検索の当たりの候補なら窓の中心（ほかは無し）。
    [[nodiscard]] std::optional<PreviewAnchor>
    preview_anchor_of(const core::CommandPalette &palette, const core::CommandChoice &choice) const;
    // 選んでいる候補が port で読む窓を要るならその要求（券は 0・段 P2）。
    [[nodiscard]] std::optional<PreviewRequest>
    preview_request_of(const core::CommandPalette &palette) const;
    void held_preview(const PreviewKey &key, PreviewSnippet &snippet) const;
    // 意図の出口で要求と取消を決める 1 本と、WorkCompleted の collect の 1 本（決定 4）。
    void settle_finder_preview();
    void collect_finder_preview();
    void forget_finder_previews();
    // ---- 全文検索（ADR 0167・EditorControllerProjectSearch.cpp）
    // 意図の出口と面を開く道の 1 本（決定 1）。面が `!` の出どころで検索語が前と違えば、前の券を
    // 取り消し、前の当たりを面から外し、新しい券で頼む（検索語が空・解析できない・探す場所が無い
    // なら頼まずに状態の 1 行だけ）。面が閉じた・ほかの出どころへ移ったら取り消して控えを消す。
    void settle_project_search();
    void start_project_search(std::string_view query);
    // 今の券の要求がまだ終わっていなければ cancel する。
    void cancel_project_search();
    // 探す場所（決定 2）。設定のルート、無ければアクティブな文書のフォルダ、無題なら値なし。
    [[nodiscard]] std::optional<core::FilePath> project_search_root() const;
    // WorkCompleted の 1 本（決定 4）。今の券の batch の当たりを面の後ろへ足し、進み具合を写す。
    void collect_project_search();
    void adopt_project_batch(const ProjectSearchBatch &batch,
                             std::vector<core::CommandChoice> &more);
    // 面の当たりの index 番目を開く（決定 5・open_listed の 1 本）。
    void open_project_hit(std::size_t index);
    // 開いた当たりの文書がアクティブならキャレットを置く（`''` を置いてから
    // PlaceCaret）。開く要求が 消えていれば（開けなかった）置く先を捨てる。
    void land_project_hit();
    // 飛ぶ前の位置を `''` とジャンプの列に置く 1 本（全文検索の当たり・定義・診断の跳び）。
    void set_context_mark();
    // 着地の位置 → アクティブな本文の行と桁（選択肢ごとの写し先を std::visit で引く・言語
    // サーバーの Position は lsp_offset_of の 1 本）。
    [[nodiscard]] core::TextPosition landed_position(const LandingPlace &place) const;
    [[nodiscard]] core::TextPosition landed_position(const core::TextPosition &position) const;
    [[nodiscard]] core::TextPosition landed_position(const LspLanding &landing) const;
    // 進み具合の表示値（面が `!` の出どころのときだけ）。
    [[nodiscard]] std::optional<ProjectSearchView> project_search_view() const;
    // 見えている当たりの行の表示値（面が `!` の出どころのときだけ・rows と同じ窓）。
    [[nodiscard]] std::vector<core::ProjectHitRow> project_hit_rows(std::size_t first,
                                                                    std::size_t count) const;
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
    // 通常モードの検索・置換・行へ移動（ADR 0174）。Vim モードは入力行の前置き（決定 2）。
    void accept(const BeginFind &);
    void accept(const BeginGoToLine &);
    void accept(const BeginReplace &);
    // Vim モードで一覧から選んだ操作の鍵を 1 つ engine へ流す（`/` `:` `n` `N`・
    // 決定 2）。INSERT なら先に <Esc> で NORMAL へ戻す（INSERT の `/` は文字）。
    std::optional<core::VimRepeatFailure> step_vim_command(core::VimKey key);
    // 「検索: 」の確定と F3 / Shift+F3（決定 3・6）。語 typed（打ったまま）を逃がして engine の
    // 検索の鍵 1 つにし、入力行は閉じない。confirmed は Enter（断りの音を鳴らす）。
    std::optional<core::VimRepeatFailure>
    hop_find(std::string_view typed, core::VimSearchDirection relative, bool confirmed);
    // Ctrl+F（phase は find）と Ctrl+H（replace_with）の「検索: 」を前回の語で開く（決定 3）。
    void open_find(FindPhase phase);
    // 置換の流れの「検索: 」の Enter（決定 3・7）。今の当たりへ移ってから「置換後: 」を開く。
    // 見つからなければ「検索: 」のまま E486。
    std::optional<core::VimRepeatFailure> advance_to_replacement(const core::SearchLine &line);
    // 「置換後: 」の Enter（決定 7）。今の当たりを 1 件置き換える Edit 1 回 → 次の当たりへ。
    std::optional<core::VimRepeatFailure> replace_current(const core::SearchLine &line);
    // 「置換後: 」の Ctrl+Enter（決定 7）。core の substituted_all の Edit 1 回。ほかの入力行では
    // 何もしない。
    void replace_all_matches();
    // 置換の語 FindState.pattern を逃がして今の大小の旗で解いた照合器。空・解けなければ値なし。
    [[nodiscard]] std::optional<core::VimPattern> find_pattern() const;
    // 次の検索の起点を at の直前の文字に置く（at から始まる当たりを飛ばさない・決定 7）。置換の
    // Edit の後はキャレット、「置換後: 」へ進むときは今の当たりの始まり。
    void restart_find_before(core::Offset at);
    // 入力行の無い F3 / Shift+F3 と一覧の「次を検索」「前を検索」（決定 2・6）。本文の順の向きを
    // 最後の検索の向きと比べて `n` か `N` を流す。
    void repeat_find(core::VimSearchDirection relative);
    // 「行: 」の確定（決定 8）。`:N` と同じ着地へ行き、桁があれば行の中へ寄せる。
    void go_to_line(const core::SearchLine &line);
    // タブ（ADR 0056 の決定 3・4・6）。切り替えは leave_document → EditorState の置く・広げる →
    // enter_document の順で、どの意図もこの 1 本を通る。
    void accept(const NewTab &);
    void accept(const SwitchTab &intent);
    // 使った順の歩きと確定（ADR 0058 の決定 3）。歩きの 1 歩も上の切り替えと同じ 1 本を通る。
    void accept(const WalkRecentTab &intent);
    void accept(const SettleRecentTab &);
    void accept(const CloseTab &intent);
    // 題名のダブルクリック（ADR 0179 の決定 4-4）。そのタブへ切り替え、切り替えが成ったタブが
    // プレビューのタブなら印を消す（通し番号で比べる・#1427 の L1）。
    void accept(const PinTab &intent);
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
    void accept(const ResizeExplorer &intent);
    void accept(const ResizeSplit &intent);
    void accept(const ResizePane &intent);
    // マウスを載せた面の行（#1390）。使う人の操作ではない（帯の PointTitleBar と同じ側）。
    void accept(const HoverExplorer &intent);
    [[nodiscard]] std::vector<core::FilePath> unsaved_files() const;
    [[nodiscard]] ExplorerRowView explorer_row_view(const core::ExplorerRow &row,
                                                    std::span<const core::FilePath> unsaved) const;
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
    // 1 回のクリック（opened_as は transient）と、1 打目が効かなかった 2 打目（pinned）の 1 本。
    void click_explorer_row(std::size_t row, OpenedAs opened_as);
    void double_click_explorer(std::size_t row);
    void second_click_explorer(const core::FilePath &path);
    void open_explorer_folder(std::optional<std::size_t> row);
    void enter_explorer_row(std::optional<std::size_t> row);
    void collapse_explorer_row();
    // 設定が変わった後（persist_settings）と起動の 1 か所。木をルートに揃え、読むものを頼む。
    void sync_explorer();
    // ミニマップ（ADR 0171 の決定 1 と追補）。設定が変わった後（persist_settings）と起動の 1 か所
    // だけが sync_minimap で minimap_shown_ を置く。編集の 3 か所（replace・undo・redo）の
    // note_minimap_edit と frame の 1 か所の add_minimap は、オフなら最初の 1 行で返る
    // （forget_syntax_before・explorer_view と同じ早い return）。本体は
    // EditorControllerMinimap.cpp。
    void sync_minimap();
    void note_minimap_edit(const core::TextBuffer &after, const core::Edit &edit);
    void add_minimap(EditorFrame &result);
    // 面の上の押しとドラッグ（決定 5 の追補）。本文の窓だけを送る（ScrollLines と同じ道）。
    void accept(const PointMinimap &intent);
    [[nodiscard]] bool grab_minimap(MinimapPointing pointing, AutosaveId tab);
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
    // 表示言語の候補の面（ADR 0123 の追補 2026-11-13 の決定 8）。Ctrl+P の面を `:set language=` で
    // 開く。
    void accept(const OpenDisplayLanguageList &intent);
    [[nodiscard]] std::pair<std::vector<core::CommandChoice>, std::size_t>
    format_choices(core::DocumentFormatKind kind) const;
    // 枠（ADR 0148）。分ける・移る・閉じる・もう 1 つの枠を送る。
    void accept(const SplitPane &intent);
    void accept(const SwitchPane &);
    void accept(const ClosePane &);
    void accept(const ScrollPane &intent);
    // Markdown のプレビュー（ADR 0155）。見せ方の巡回は使う人の操作（決定 3・追補 #1354）、tick
    // は間引きの合図で使う人の操作ではない（決定 8）。
    void accept(const TogglePreview &);
    void accept(const PreviewTick &);
    // チェックボックスの切り替え（ADR 0155 の追補 24）。使う人の操作で、本文の編集 1 回。
    void accept(const TogglePreviewTask &intent);
    // 見せ方を選ぶ（ADR 0155 の改訂 16）。F1 の一覧と `:preview` の 1 本。
    void accept(const SetPreviewPlacement &intent);
    void place_preview(PreviewPlacement placement);
    // off から側の枠を開く（決定 3）。place_preview の side の 1 か所から呼ぶ。
    void open_side_preview();
    // 全面をやめて placement（off か side）へ。本文の見える位置を錨の行に揃える（改訂 17）。
    void leave_full_preview(PreviewPlacement placement);
    // `:preview?` と `:preview full|side|off` の答え（今の見せ方を 1 行）。
    void answer_preview(core::ExPreviewRequest request);
    // 全面の間の意図の振り分け（改訂 17）。入口の 1 か所で、写さずに受けたら真。
    [[nodiscard]] bool route_full_preview(const EditorIntent &intent);
    void run_full_preview_route(PreviewFullRoute route, const EditorIntent &intent);
    void finish_vim_g(const VimKeyPress &press);
    // 見せているプレビューの送りの量に lines（プレビューの行の単位）を足し、追従をやめる。畳むのは
    // ui（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。追従中なら本文の先頭行の行頭から始める。
    void scroll_preview(std::int64_t lines);
    // 錨を本文の行 line の行頭へ置き直し、追従をやめる（`gg` `G` のような絶対の行き先・nudge と
    // pending は 0）。
    void anchor_preview_at(core::LineNumber line);
    // ui が畳んだ送り（#1894）。合図で、使う人の操作ではない（is_signal）。
    void accept(const PreviewScrollSettled &intent);
    // 全面で通常モードの Space（追補 25）。キャレットの行のタスクを切り替え、無ければ知らせる。
    void toggle_caret_task();
    // 全面の選択とコピー（改訂 18）。select_preview は preview_full_route が select と決めた意図
    // （Shift+矢印・マウス）だけを受け、Ctrl+A と Vim の `Ctrl-a` は select_all、Ctrl+C と Vim の
    // `y` は copy の行き先が select_all_preview と copy_preview_selection を呼ぶ。
    // PointPreview は全面でなければ何もしない。
    void accept(const PointPreview &intent);
    void select_preview(const EditorIntent &intent);
    void point_preview_selection(const PointPreview &intent);
    void move_preview_selection(core::CaretMotion motion);
    void select_all_preview();
    void copy_preview_selection();
    // プレビューが開いていれば、今の本文の解析を頼む（決定 3・6・9）。fresh は操作中の文書が
    // 替わったときで、前の結果と知らせを捨てて追従に戻す。16 MiB を超える本文は頼まず知らせる。
    void request_preview(bool fresh);
    // プレビューが開いていれば今の券の結果を引く（決定 6・10）。WorkCompleted の道だけが呼ぶ。
    void collect_preview();
    // プレビューが開いていれば、錨の要素から見える範囲の画像を錨に近い順に 1 つ頼む（ADR 0155 の追
    // 補 50・51）。口に頼み中があれば頼まない。解析の結果が届いたとき・画像の結果を引いたとき・
    // PreviewTick・錨を送ったときだけ呼ぶ。画像の節が無ければ何もしない。
    void request_preview_images();
    // プレビューが開いていれば画像の結果を引き、次の 1 つを頼む（追補 50）。WorkCompleted の道だけ
    // が呼ぶ。
    void collect_preview_images();
    // 画像の口の結果を全部引き、頼み中の券の結果があれば頼み中を解く（引く所はここの 1 本）。
    [[nodiscard]] std::vector<ImageOutcome> take_preview_images();
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
    // 席（2026-11-20 の追補・段 S2）: serving はその言語の文書を受ける席、active_language_server
    // はアクティブな文書を受ける席のつながり（無ければ空）、holding はアクティブな文書を開いた席。
    // 失敗の数と given_up は言語ごと（trouble_of）。席が満ちたら yield
    // が最も前に使った席を譲らせる。
    void reconcile_language_server();
    [[nodiscard]] std::optional<LanguageServerSlot> serving_slot(core::Language language) const;
    [[nodiscard]] const LanguageServerLink &active_language_server() const;
    [[nodiscard]] std::optional<LanguageServerSlot> holding_slot() const;
    [[nodiscard]] std::optional<LanguageServerSlot> free_slot() const;
    [[nodiscard]] LanguageServerLink &link_of(LanguageServerSlot slot);
    [[nodiscard]] LanguageServerTrouble &trouble_of(core::Language language);
    [[nodiscard]] bool language_server_given_up(core::Language language) const;
    void resume_language_servers();
    void consider_language_server(core::Language language);
    void yield_language_server();
    [[nodiscard]] std::optional<ServerLaunch> wanted_language_server(core::Language language);
    // root（2026-11-22 の追補・段 S3・#1960）: document_root はアクティブな文書の root（設定の root
    // を上限に印を探した答え → 設定の root か文書のフォルダ・まだ分からなければ理由）、seek は印を
    // 探す頼みを 1 回だけ出し、collect は答えを覚えて決め直す（頼んでいなければ port を呼ばない）。
    // held_root は reconcile が hold_language_root で 1 回決めた値が今も同じ鍵なら読み、違えば
    // document_root（#1944 のレビュー L1）。
    [[nodiscard]] std::expected<core::FilePath, LanguageRootGap>
    document_root(core::Language language) const;
    [[nodiscard]] std::expected<core::FilePath, LanguageRootGap>
    held_root(core::Language language) const;
    void hold_language_root(core::Language language);
    [[nodiscard]] bool holds_language_root(core::Language language) const;
    void seek_language_root(core::Language language);
    void collect_language_roots();
    [[nodiscard]] bool has_language_server_command(core::Language language) const;
    [[nodiscard]] std::optional<ServerLaunch>
    language_server_launch(core::Language language, const core::FilePath &root) const;
    void start_language_server(LanguageServerSlot slot, core::Language language,
                               ServerLaunch launch);
    // 席 slot がアクティブな文書を受ける・開いているなら、補完・hover・定義の答えを消す（止める・
    // 譲る道の入口で呼ぶ・ほかの席を止めてもアクティブな文書の答えは残す・#1916 のレビュー R1）。
    void drop_language_answers_of(LanguageServerSlot slot);
    void stop_language_server(LanguageServerSlot slot);
    void shut_language_server(LanguageServerSlot slot);
    void send_lsp_request(LanguageServerSlot slot, LspRequestKind kind,
                          const core::JsonValue &params, PendingLspContext context = {});
    void send_lsp_cancel(LanguageServerSlot slot, std::int64_t id);
    void language_server_failed(LanguageServerSlot slot, core::UiMessage message);
    void collect_language_server();
    void receive_server(LanguageServerSlot slot, const ServerStarted &started);
    void receive_server(LanguageServerSlot slot, const ServerMessage &message);
    void receive_server(LanguageServerSlot slot, const ServerExited &exited);
    void receive_server(LanguageServerSlot slot, const ServerFailed &failed);
    // 標準エラーの塊は記録へ（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。
    void receive_server(LanguageServerSlot slot, const ServerErrorOutput &output);
    void end_language_server(LanguageServerSlot slot);
    void receive_lsp(LanguageServerSlot slot, const core::LspResponse &response);
    void receive_lsp(LanguageServerSlot slot, const core::LspNotification &notification);
    void receive_lsp(LanguageServerSlot slot, const core::LspRequest &request);
    // `$/progress` の 1 通を席の token ごとの今の値へ写す（追補）。
    void receive_progress(LanguageServerSlot slot, core::LspProgressValue value);
    // 席 slot の言語（記録の行の頭・止めた後の残りは今の文書の言語）。
    [[nodiscard]] core::Language slot_language(LanguageServerSlot slot) const;
    // 読まずに捨てる出来事のうち記録に残すもの（#1950 のレビュー M1）。止めた席の port が返した
    // 残りと、同じ集めの中で止めた後に来た出来事の、標準エラーの塊と showMessage / logMessage
    // だけを language の行として記録に足す。状態・知らせ・答えは動かさない。
    void log_unread(LanguageServerSlot slot, core::Language language, const ServerEvent &event);
    // 記録を無題のタブに写して開く（追補・F1 の一覧と `:LspLog`）。先にたまった出来事を集め、
    // 記録が空なら 1 行知らせて開かない。
    void open_language_server_log();
    void accept(const OpenLanguageServerLog &);
    // アクティブな文書を受ける席の進み具合か起動中の 1 行（追補）。frame と hit test が同じ値を
    // 引く（language_server_status）。
    void add_language_server_status(EditorFrame &result) const;
    void accept(const LanguageServerTick &);
    void age_lsp_requests(LanguageServerSlot slot);
    void expire_lsp_request(LanguageServerSlot slot, const PendingLspRequest &request);
    void answer_lsp(core::ExLspRequest request);
    // `:set language=` `:set language?`（ADR 0123 の追補 2026-11-13 の決定 8）。
    void answer_display_language(const core::ExDisplayLanguageRequest &request);
    // 文の表の 1 行を今の表示言語で（#1019）。values は置き場 {0} {1} を埋める値。
    [[nodiscard]] core::DisplayText ui_notice(core::UiMessage message) const;
    [[nodiscard]] core::DisplayText ui_notice(core::UiMessage message,
                                              std::initializer_list<std::string_view> values) const;
    void end_language_server_session();
    // 文書の同期と診断（ADR 0161 の決定 1〜3・5）。sync は running の間の apply の終わりで、閉じた
    // タブ・言語や URI や大きさの変わったアクティブな文書を `didClose` し、この意図の編集を文書
    // ごとに 1 通の `didChange` に束ね、送っていないアクティブな文書を `didOpen` する。close_all は
    // `shutdown` の前（notify）と stop（送らない）で全部を閉じて診断を片付ける。edit は編集の
    // 1 本（replace・undo・redo）が本文を置く前に呼び、診断をずらし、開いた文書なら変更を積む。
    void sync_language_server_documents(LanguageServerSlot slot, bool serves);
    void close_language_server_documents(LanguageServerSlot slot, bool notify);
    void close_language_server_document(LanguageServerSlot slot,
                                        const LanguageServerDocument &document, bool notify);
    void open_language_server_document(LanguageServerSlot slot, AutosaveId tab, std::string uri);
    void flush_language_server_changes(LanguageServerSlot slot, LanguageServerDocument &document);
    [[nodiscard]] std::optional<std::string> language_server_uri(LanguageServerSlot slot) const;
    // アクティブな文書を言語サーバーへ送れる形か（パスがあり本文が上限以下・言語は問わない）。
    [[nodiscard]] bool language_server_sendable() const;
    void language_server_edit(const core::Edit &edit);
    // 保存の成功の後（ADR 0161 の 2026-11-20 の追補）。サーバーが `save` を求め、アクティブな文書を
    // 保存したパスの URI のまま開いていれば、積んだ変更を流してから `didSave` を 1 通送る。
    void language_server_saved(const core::FilePath &path);
    [[nodiscard]] bool same_document_uri(std::string_view sent, std::string_view answered) const;
    void receive_diagnostics(LanguageServerSlot slot, const core::LspNotification &notification);
    void place_diagnostics(AutosaveId tab, std::vector<core::Diagnostic> diagnostics,
                           std::uint64_t version, core::DiagnosticsCount count);
    // 文脈つきの要求と答え（ADR 0190 の決定 1〜3）。本体は EditorControllerLanguageRequests.cpp。
    // requestable は要求を出せるアクティブな文書（running・能力あり・`didOpen` 済み・URI が今の
    // まま）で、未送信の変更を流してから返す。send_language_request は同じ種類の古い要求を
    // 取り消して送る。receive_answer は文脈を照合して答えを状態に置く（合わなければ捨てる・
    // 知らせない）。
    [[nodiscard]] std::optional<LanguageServerSlot>
    requestable_slot(bool ServerAbilities::*ability);
    [[nodiscard]] LanguageServerDocument &active_document_of(LanguageServerSlot slot);
    void send_language_request(LanguageServerSlot slot, LspRequestKind kind, core::JsonValue params,
                               PendingLspContext context);
    void cancel_lsp_requests(LanguageServerSlot slot, LspRequestKind kind);
    [[nodiscard]] core::LspPosition request_position(LanguageServerSlot slot) const;
    [[nodiscard]] LspRequestContext request_context() const;
    void receive_answer(LanguageServerSlot slot, const PendingLspRequest &request,
                        const core::LspResponse &response);
    // 問いと答えの使い方（ADR 0190 の決定 6〜8）。本体は EditorControllerLanguageQueries.cpp。
    // query_language は操作（F12・情報を表示）と Vim の `gd` `K` の効果が入る 1 本で、出せたら真。
    // settle_language_answers は答えが届く合図の道の中で、空の hover を知らせ、定義の答えを 1 回
    // 使って消す。hover の札は答えを状態に置いたまま shown_hover が frame に写し、消すのは入口の
    // settle_hover（使う人の操作の意図）と WindowDeactivated の 2 か所。
    void accept(const QueryLanguage &intent);
    void accept(const WindowDeactivated &);
    void perform(const core::VimLanguageQuery &effect);
    [[nodiscard]] bool query_language(core::LanguageQueryKind kind);
    void settle_hover(const EditorIntent &intent);
    void drop_hover();
    // 引数の情報の札（#1959・EditorControllerSignature.cpp）。
    void settle_typed_signature(std::string_view typed);
    void settle_signature_answer();
    void settle_signature_tip();
    [[nodiscard]] bool signature_allowed() const;
    [[nodiscard]] bool invoke_signature_help();
    [[nodiscard]] std::optional<core::LspSignature> shown_signature(const EditorFrame &frame) const;
    void drop_signature_tip() noexcept;
    // 整形の答えを当てる（#1993・EditorControllerFormatting.cpp）。
    void settle_formatting_answer();
    [[nodiscard]] core::OffsetRange formatting_range() const;
    void leave_visual_for_formatting();
    [[nodiscard]] std::optional<std::string> shown_hover() const;
    // 診断の跳び・札・一覧（#1939・EditorControllerDiagnostics.cpp）。操作（F8・Shift+F8・診断を
    // 表示・診断の一覧）と Vim の `]d` `[d` `gl` の効果が run_diagnostic_command の 1 本へ入る。
    // 札は hover の札と同じ欄（LanguageAnswers の hover）に置き、同じ道（settle_hover）で消える。
    // 一覧は面を開くときに append_diagnostic_choices が候補と palette_diagnostics_ を 1 回作る。
    // run_diagnostic_command は跳べた・札を出せた・一覧を開いたときに真（Vim の鍵の成功の歩）。
    void accept(const RunDiagnosticCommand &intent);
    void perform(const core::VimDiagnosticCommand &effect);
    [[nodiscard]] bool run_diagnostic_command(core::DiagnosticCommand command, std::size_t count);
    [[nodiscard]] bool step_diagnostic(bool forward, std::size_t count);
    [[nodiscard]] bool show_diagnostic_tip(core::Offset at);
    void open_diagnostic_list();
    void append_diagnostic_choices(std::vector<core::CommandChoice> &entries);
    [[nodiscard]] std::span<const core::Diagnostic> tab_diagnostics(std::size_t tab) const;
    void append_tab_diagnostics(std::size_t tab, std::string_view name,
                                std::vector<core::CommandChoice> &entries);
    [[nodiscard]] std::optional<PaletteDiagnostic>
    palette_diagnostic_of(const core::CommandChoice &choice) const;
    void land_palette_diagnostic(const core::CommandChoice &choice);
    // 参照の一覧（#1947・ADR 0191 の決定 4・EditorControllerReferences.cpp）。答えは
    // held_references_ に控えて Ctrl+P の一覧を記号 `&` で開き、`&` を見せるときに
    // append_reference_choices が候補と palette_references_ を 1 回作る。選ぶと定義と同じ着地の 1
    // 本（land_location）。
    void settle_references_answer();
    void append_reference_choices(std::vector<core::CommandChoice> &entries);
    [[nodiscard]] std::optional<PaletteReference>
    palette_reference_of(const core::CommandChoice &choice) const;
    void land_palette_reference(const core::CommandChoice &choice);
    void land_palette_choice(const core::CommandChoice &choice);
    // 記号の一覧（#1955・ADR 0191 の決定 5・同じ EditorControllerReferences.cpp）。答えは
    // held_symbols_ に控えて記号 `%` で開き、候補の指す所は参照と同じ形の
    // palette_symbols_（選ぶ道と プレビュー欄は palette_reference_of が出どころで列を選ぶ 1 本）。
    void settle_symbols_answer();
    void use_symbols_answer(LanguageSymbols answered, bool listing);
    void keep_symbols_answer(LanguageSymbols answered);
    void append_symbol_choices(std::vector<core::CommandChoice> &entries);
    // 参照と記号の候補は、その記号を見せるときにだけ作り、同じ一覧の間は 1 回だけ作って持つ
    // （#1958 のレビュー R1）。`%` は今の文書の今の改訂の答えだけを見せ、違えば頼み直す（決め 4）。
    void settle_listed_answers();
    void show_held_symbols(bool listing);
    void reset_listed_answers();
    void extend_palette(const std::vector<core::CommandChoice> &more);
    [[nodiscard]] bool palette_scope_is(core::PaletteScope scope) const;
    [[nodiscard]] bool held_symbols_current() const;
    [[nodiscard]] const std::vector<PaletteReference> *
    listed_references_of(core::PaletteOrigin origin) const noexcept;
    // 一覧の中の空の 1 行と、参照の一覧が何を頼んだ答えか（#1958 の決め 2・3）。
    [[nodiscard]] std::optional<core::DisplayText>
    palette_empty_line(core::PaletteScope scope) const;
    [[nodiscard]] std::optional<core::DisplayText> requested_subject() const;
    [[nodiscard]] std::optional<core::DisplayText>
    held_references_subject(core::PaletteScope scope) const;
    void fill_palette_hint(CommandPaletteView &view, const core::CommandPalette &palette) const;
    // 参照の答えを控えた文書を言語サーバーが閉じたら控えを捨てる（#1954 のレビュー R1）。
    void forget_listed_answers(AutosaveId document);
    // URI → ファイルと開いているタブの本文（URI が変わったときだけ引く）。
    using ListedFile = std::pair<std::optional<core::FilePath>, const core::TextBuffer *>;
    [[nodiscard]] ListedFile listed_file_of(const std::string &uri) const;
    [[nodiscard]] const ListedFile &
    listed_file_in(std::vector<std::pair<std::string_view, ListedFile>> &seen,
                   const std::string &uri) const;
    // 言語サーバーは動いているが、アクティブな文書を受ける席がその能力を持たない（決定 2）。
    [[nodiscard]] bool lacks_ability(bool ServerAbilities::*ability) const;
    [[nodiscard]] core::UiMessage unanswered_notice(core::LanguageQueryKind kind) const;
    // プレビューのリンクの札（施主決定 D105・D106・#1869 の段 L2・EditorControllerLinkCard.cpp）。
    // 開くのは ClickPreviewLink、閉じるのは札の意図と入口の close_link_card_before（札の意図と
    // 使う人の操作でない意図のほか）。web と mail は LinkOpenPort、document は open_listed の 1
    // 本。
    void accept(const ClickPreviewLink &intent);
    void accept(const ActOnLinkCard &intent);
    void close_link_card_before(const EditorIntent &intent);
    // 札を開いたタブがアクティブでなくなったら札を消す（#1876 のレビュー R1）。
    void drop_stale_link_card();
    [[nodiscard]] AutosaveId active_tab_id() const;
    // frame に載せる札（見せている結果の同じ要素に同じリンクがある間だけ）。
    [[nodiscard]] std::optional<LinkCardView> shown_link_card() const;
    void open_link_card(const LinkCardView &card);
    void open_link_document(std::string_view path);
    void copy_link_card(const LinkCardView &card);
    // 同じ文書の `#見出し` のリンク（段 L3）。名札の列で引いた見出しの元の行の頭へキャレットを置き
    // （`''` に前の位置）、全面ではプレビューの送りもその見出しへ。見つからなければ 1 行知らせる。
    void jump_to_heading(std::string_view fragment);
    // WorkCompleted の道で開く頼みの結果を引き、失敗を 1 行知らせる（頼んでいなければ呼ばない）。
    void collect_link_opens();
    // プレビューの表の横の送り（#1883・施主決定 D107・EditorControllerPreview.cpp）。送りの量は
    // 文書の値（EditorState::table_scrolls）、つかみとマウスを載せた棒は窓の値。
    void accept(const ScrollPreviewTable &intent);
    void accept(const HoverPreviewTable &intent);
    void adopt_table_scrolls(std::size_t tables);
    void drop_table_pointer();
    [[nodiscard]] std::optional<std::int32_t>
    table_scroll_moved(const ScrollPreviewTable &intent, std::int32_t from,
                       const std::optional<PreviewTableGrip> &grip);
    // つまみを明るく描く表（つかんでいる表、無ければマウスを載せた棒の表）。
    [[nodiscard]] std::optional<std::size_t> preview_table_lit() const;
    void settle_language_answers(LspRequestKind kind);
    void settle_hover_answer();
    void settle_definition_answer();
    void land_definition(const LanguageDefinition &definition);
    // file の at へ跳ぶ 1 本（定義と参照・開いていなければ開いてから・`''` を置く）。
    void land_location(const core::FilePath &file, const LspLanding &at);
    // 答えを使ってよい状態か（#1843 のレビュー R1〜R3）。faces_closed は入力行・面・右クリックの
    // メニュー・IME の変換・補完の面が開いていない。awaits_nothing
    // は問いを出したときと同じ「何も待って いない」状態（Vim は NORMAL
    // でオペレータ・回数・レジスタ・次の鍵の待ちが無い／通常モードは
    // 選択の起点が問いのときのまま）。
    [[nodiscard]] bool faces_closed() const noexcept;
    [[nodiscard]] bool awaits_nothing() const noexcept;
    // 自動退避の合図（ADR 0137 の決定 1・2）。WorkCompleted と同じく使う人の操作の途中の状態を
    // 動かさない。
    void accept(const AutosaveTick &);
    // 計画を回して store と discard を頼み、帳面を進める（決定 2）。合図と復元の直後が呼ぶ。
    void autosave_now();
    // 計画の仕事を頼み、帳面を進める（autosave_now と back_up_before_defect の 1 本）。上限を超えて
    // 書かない文書があれば偽。
    bool run_autosave_steps(const std::vector<std::pair<AutosaveId, AutosaveStep>> &plan);
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
    // タブの札（#1789・ADR 0056 の #1789 の追補）。合図で待っていたタブにまだ載っていれば出す。
    // 消すのは入口の settle_tab_tip の 1 か所で、使う人の操作の意図（not_user_operation でない
    // 意図）が来たら写す前に消す。札を出しているタブは shown_tab_tip が frame に写す。
    void accept(const TabTipTick &intent);
    // キャレットの動きの合図（ADR 0192 の決定 3）。進みを覚えて frame に載せるだけで、状態を
    // 変えない。
    void accept(const CaretMotionTick &intent);
    void settle_tab_tip(const EditorIntent &intent);
    void hide_tab_tip();
    [[nodiscard]] std::optional<std::size_t> shown_tab_tip() const;
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
    // `:s///c` の問いの Enter は答えではない（Vim は無視する・ADR 0168 の決定 6）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> submit(const SubstituteQuestion &question);
    [[nodiscard]] std::optional<core::VimRepeatFailure> submit(const JumpCharacter &line);
    [[nodiscard]] std::optional<core::VimRepeatFailure> submitted_command();
    [[nodiscard]] std::optional<core::VimRepeatFailure> confirmed_command();
    // あふれた入力の確定を断ったら真（入力行を閉じて知らせる・#1568）。
    [[nodiscard]] bool refused_overflow();
    void remember(const core::CommandLine &line);
    void remember(const core::SearchLine &line);
    void remember(const core::CommandPalette &palette);
    void remember(const SubstituteQuestion &question);
    void remember(const JumpCharacter &line);
    // `:s///c` の問い（ADR 0168 の決定 6・本体は EditorControllerCommand.cpp と畳みの
    // EditorControllerVim.cpp）。開いている問い（無ければ空）・文字を答えとして読む（問いが開いて
    // いれば真）・答えを engine の 1 歩で写す・答え以外の意図の前に `q` と同じに終える（入口の
    // 1 か所）・終えるときに置いた分を 1 つの undo の単位に畳む（改訂番号は問いの間のどの値より
    // 大きい・ADR 0168 の追補 段 2a）。
    [[nodiscard]] const SubstituteQuestion *substitute_question() const noexcept;
    [[nodiscard]] bool answered_question(std::string_view utf8);
    // イージーモーション（ADR 0163・EditorControllerEasyMotion.cpp）。1
    // 文字の入力行の文字を跳び先の
    // 文字として流す（行が開いていれば真）・意図の出口で行を開閉して印を捨てるかを決める 1 か所。
    [[nodiscard]] bool answered_jump(std::string_view utf8);
    void settle_easymotion(const EditorIntent &intent);
    void settle_jump_line();
    void settle_labels(const EditorIntent &intent);
    void dropped_easymotion();
    void answer_question(core::SubstituteReply reply);
    void record_question_key(const core::VimKey &key);
    void end_question_before(const EditorIntent &intent);
    void merge_question_edits(const SubstituteQuestion &question, core::Offset restore);
    void submit_palette(const core::CommandPalette &palette);
    [[nodiscard]] std::optional<core::VimRepeatFailure> evaluate_command(std::string_view text);
    [[nodiscard]] std::expected<core::ExResult, core::ExEvaluationFailure>
    evaluated_ex(std::string_view text) const;
    // オペレータ待ちの `:` の確定（ADR 0022 の追補 #1549）。
    [[nodiscard]] std::optional<core::VimRepeatFailure> operator_command(const std::string &text);
    [[nodiscard]] bool refused_operator_command(
        std::string_view text,
        const std::expected<core::ExResult, core::ExEvaluationFailure> &result);
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    operator_closed_in_place(const std::string &text);
    // オペレータ待ちの `:s` `:d` `:y` `:g`（ADR 0168 の追補 16 と段 3a）。命令の前のキャレットから
    // 後のキャレットまで。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    operator_line_edited(const std::string &text, const VimStepSource &source);
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    ran_ex_result(std::string_view text,
                  const std::expected<core::ExResult, core::ExEvaluationFailure> &result);
    // 写した答えの知らせと、複数の引数の `:set` が途中で止まった断り（#1561）。
    [[nodiscard]] std::optional<core::VimRepeatFailure>
    noted_ex_result(std::string_view text, const core::ExResult &result);
    // Vim の入力行（`:` `/` `?`）が開かれた後、意図の出口で入力行が閉じているか別の面に置き換わって
    // いれば、保留のオペレータと始め直していない `Ctrl-o` の印を捨てる 1 か所（#1549・#1569）。
    void settle_vim_line();
    [[nodiscard]] bool vim_line_shown() const;
    // controller が鍵の列の後で窓に頼む要求（閉じる・操作）が立っているか（#1569 の M2）。
    [[nodiscard]] bool window_request_pending() const noexcept;
    [[nodiscard]] std::optional<core::ExLineRequest>
    landing_line(const core::ExResult &result) const;
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
    void jump_below_caret(const core::ExRelativeLineRequest &request);
    [[nodiscard]] std::optional<core::ExLineRequest>
    line_below_caret(const core::ExRelativeLineRequest &request) const;
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
    // 印刷（ADR 0177 の決定 1・2・7）。PrintPort を呼ぶのはこの意図の 1 か所だけで、`:hardcopy` も
    // 同じ道へ渡す。取り消しは黙り、失敗と大きすぎる本文は 1 行知らせる（鳴らさない）。
    void accept(const PrintDocument &);
    void run_print_request(core::ExPrintRequest request);
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
    // 設定を状態に置いて後ろの同期（エクスプローラー・ミニマップ・map の表・追従）を通す 1 本。
    // persist_settings が書いた後と、設定の面の切り替え（保存しない）が共用する。
    void adopt_settings(core::EditorSettings settings);
    // 設定を port に書く 1 本（persist_settings と設定の面の閉じが共用）。失敗は状態に置いて偽、
    // 成功は失敗を消して真。状態の設定は変えない。
    [[nodiscard]] bool write_settings(const core::EditorSettings &settings);
    // 設定が変わった後の追従と clamp の頼み（順は定義のコメント・#1432）。
    void follow_after_settings(bool reshaped);
    // Vim が窓を決め直す option（`scrolloff` `wrap` `relativenumber` `hlsearch`）を置いた時点の
    // キャレットで追従を頼み、印を立てる（#1432・#1524）。
    void follow_window_setting();
    // IME の 3 つ（ADR 0014 の決定 3）。ComposeText と CancelComposition は本文にも履歴にも
    // 触らず、CommitText だけが既存の 1 本（replace / vim_step）を通って本文に入る。
    void accept(const ComposeText &intent);
    void accept(const CommitText &intent);
    void accept(const CancelComposition &);
    // F5 の日時の挿入（ADR 0175 の決定 3）。時刻はここだけで ClockPort から読む（決定 4）。入力行・
    // 通常モード・Vim INSERT は IME の確定と同じ道で入れ、Vim のほかのモードは 1 行知らせる。
    void accept(const InsertDateTime &);
    // エクスプローラーへの登録の 2 つの操作（ADR 0178 の決定 3）。ShellRegistrationPort を呼ぶのは
    // ここだけで（決定 4）、結果は鳴らさない 1 行の知らせにする。
    void accept(const RegisterShell &);
    void accept(const UnregisterShell &);
    // 登録の結果の 1 行（鳴らさない）。
    void show_shell_notice(const std::string &text);
    // その操作が製品の形で効くか。効かなければ書かずに 1 行知らせて偽（ADR 0178 の追補 #1809）。
    [[nodiscard]] bool shell_operation_distributed(core::EditorOperation operation);
    // バージョン情報（施主決定 D90）。core::app_about_line の 1 行を知らせにするだけで、port を
    // 呼ばない（ページを開かない・通信しない）。
    void accept(const ShowVersion &);

    // ----------------------------------------------------------------
    // 右クリックのメニュー（#1051） EditorControllerContextMenu.cpp の群だけが読み書きする（ADR
    // 0078 の追補）。
    void accept(const OpenContextMenu &intent);
    void accept(const PointContextMenu &intent);
    void accept(const StepContextMenu &intent);
    void accept(const ChooseContextMenu &intent);
    void accept(const CloseContextMenu &);
    // 入口の 1 か所（apply）。メニューが開いていて、意図がメニューの意図でも使う人の操作でない意図
    // でもなければ、写す前に閉じる（ホイール・タブ・モードの切り替え…）。
    void close_context_menu_before(const EditorIntent &intent);
    // 開いた所の種類（全面プレビュー・通常モード・Vim の INSERT / NORMAL / VISUAL）。
    [[nodiscard]] core::ContextMenuMode context_menu_mode() const noexcept;
    // 通常モードの右クリックが選択の外なら、開く前にキャレットをそこへ置く（D82）。
    void place_caret_for_menu(const PointText &pointed);
    // 点の本文の位置（accept(PointText) と共通の 1 本）。解けなければ layout の失敗を置いて無し。
    [[nodiscard]] std::optional<core::TextCaret> pointed_caret(const PointText &intent);
    // 選んだ行を実行する。Vim の NORMAL と VISUAL は表の Vim の鍵の列を engine に流す。
    void run_context_menu_row(const core::ContextMenuItem &item, core::ContextMenuMode mode);
    // メニューと F1 の一覧の実行の 1 本。keys が空なら operation_request、あれば待ちを捨てて流す。
    void run_operation_keys(core::EditorOperation operation, std::string_view keys);
    // タブの右クリックのメニュー（#1790）。開く・選んだ行の実行・帯が変わったら閉じる。
    void accept(const OpenTabMenu &intent);
    void accept(const CopyTabName &intent);
    void accept(const RevealTab &);
    // 退避番号 tab のタブに当てる（開いてから帯が変われば出口の settle_context_menu が閉じている
    // ので、番号は帯にある）。
    void run_tab_menu_row(core::EditorOperation operation, AutosaveId tab);
    // 選んだ行を種類ごとの道で実行する（メニューは閉じた後）。
    void run_chosen_row(const ContextMenuOpen &menu, std::size_t row);
    // タブの項目の実行（帯の位置 tab のタブに当てる・表 tab_menu_runner が引く）。
    void reveal_tab_at(std::size_t tab);
    void copy_tab_path_at(std::size_t tab);
    void copy_tab_file_name_at(std::size_t tab);
    void copy_tab_name_at(std::size_t tab, TabNamePart part);
    void save_tab_at(std::size_t tab);
    void save_tab_as_at(std::size_t tab);
    void close_tab_at(std::size_t tab);
    void close_tabs_beside_at(std::size_t tab);
    void close_tabs_right_at(std::size_t tab);
    // 切り替えてから operation_request を立てる（保存と名前を付けて保存）。切り替えが成らなければ
    // 何もしない。
    void run_on_tab(std::size_t tab, core::EditorOperation operation);
    using TabMenuRunner = void (EditorController::*)(std::size_t);
    [[nodiscard]] static TabMenuRunner tab_menu_runner(core::EditorOperation operation) noexcept;
    // 意図の出口で、タブのメニューの帯が開いたときと違っていれば閉じる（合図で読みが外れた など）。
    void settle_context_menu();
    // エクスプローラーで場所を開く頼みの失敗を受け取り、最後の 1 つを知らせる。頼んでいなければ
    // port を呼ばない。
    void collect_reveals();
    // F1 の一覧から選んだ操作。Vim の NORMAL / VISUAL の 5 つはメニューと同じ Vim の鍵の列。
    void run_listed_operation(core::EditorOperation operation);
    [[nodiscard]] std::optional<ContextMenuView> context_menu_view() const;

    // ---------------------------------------------------------------- 補完の面（ADR 0190 / 0169）
    // EditorControllerCompletion.cpp の群だけが読み書きする。
    void accept(const OpenCompletion &);
    void accept(const StepCompletion &intent);
    void accept(const ChooseCompletion &intent);
    void accept(const CloseCompletion &);
    void accept(const ScrollCompletion &intent);
    // 入口の 1 か所（apply）。面が開いていて、開いたまま受ける意図でなければ、写す前に閉じる。
    void close_completion_before(const EditorIntent &intent);
    // 出口の 1 か所（apply）。成り立たなくなった面を閉じ、届いた答えを足し、打った分で絞り直す。
    void settle_completion();
    // 使う人が打った文字が本文に入った後の言語サーバーの入口（#1962）。通常モードの InsertText と
    // Vim の INSERT の 1 文字（typed_vim_character）の両方がここを通る。呼ぶ側は「言語サーバーが
    // 起きている」の真偽の後ろで呼ぶ（ADR 0190 の決定 10）。typed は今打った文字。
    void typed_for_language(std::string_view typed);
    // `triggerCharacters` の 1 つを打ち終えたら開く（通常モードと Vim の INSERT・#1962）。呼ぶのは
    // 言語サーバーが running のときだけ（typed_for_language の 1 か所）。
    void open_triggered_completion(std::string_view typed);
    // 面を開く 1 か所。trigger が無ければ手で開いた面、あれば自動の面。
    void start_completion(std::optional<std::string_view> trigger);
    // 面を開ける状態か。triggered は打鍵で自動に開く面（Vim の INSERT で開けるのはこれだけ・
    // #1962）。
    [[nodiscard]] bool completion_startable(bool triggered) const;
    [[nodiscard]] bool completion_holds(const CompletionSession &session) const;
    void refresh_completion(CompletionSession &session);
    [[nodiscard]] bool take_completion_answer(CompletionSession &session);
    void merge_completion_answer(CompletionSession &session,
                                 const core::LspCompletionList &list) const;
    // キャレットの直前の語の頭（接頭辞の始まり・本文の語は集めない）。
    [[nodiscard]] core::Offset completion_word_start() const;
    // row 番目の候補を確定したときの編集（置き換える範囲と入れる平文）。
    [[nodiscard]] std::pair<core::OffsetRange, std::string>
    completion_edit(const CompletionSession &session, std::size_t row) const;
    // 面を閉じる 1 か所（待っている要求の取り消しと、届いていた補完の答えの消去）。
    void close_completion();
    void forget_completion_answer();
    [[nodiscard]] core::OffsetRange completion_range(core::Offset origin,
                                                     const core::CompletionEntry &entry) const;
    [[nodiscard]] std::optional<CompletionView> completion_view() const;
    // Vim の INSERT の補完（#1845・ADR 0169 の決定 3）。engine が形を持ち、controller は補完を
    // 始める鍵の直前に候補の写しを置き（offer）、鍵の後に engine の状態を面へ写す（follow）。
    void offer_vim_completion(const core::VimKey &key);
    // 自動で開いた面が出ている間の Ctrl-n / Ctrl-p で、面の候補を engine の写しにする（#1962）。
    // 写せる候補が無ければ偽（呼ぶ側は ADR 0169 の本文の語の道へ落ちる）。
    [[nodiscard]] bool offer_auto_vim_completion();
    // 言語サーバーが起きている窓で打った Vim の鍵（#1962）。accept(VimKeyPress) が「起きている」の
    // 真偽の後ろでだけ呼ぶ。自動で開いた面の鍵を先に読み、鍵を流した後に typed_vim_character。
    void typed_vim_key_with_language(const core::VimKey &key);
    // 自動で開いた面が出ている間の Ctrl-y / Ctrl-e は面を閉じるだけで Vim に渡さない（真）。
    // 文字・Backspace・Ctrl-n / Ctrl-p・Esc 以外の鍵は面を閉じてから Vim に渡す（偽）。
    [[nodiscard]] bool auto_vim_completion_took(const core::VimKey &key);
    // deliver_vim_key が engine に流す鍵ごとに typed_insert_ を書き直す（typed_watch_ の間だけ）。
    void watched_vim_key(const core::VimKey &key);
    // 打った鍵を流し終えた後、最後に流した鍵が typed_insert_ の 1 文字で、それが流す前のキャレット
    // の直後にそのまま入ったなら typed_for_language へ渡す。
    void typed_vim_character();
    void follow_vim_completion();
    void choose_vim_completion(std::optional<std::size_t> row);
    void settle_vim_completion(CompletionSession &session);
    void merge_vim_completion_answer(CompletionSession &session,
                                     const core::LspCompletionList &list);
    [[nodiscard]] std::optional<core::LspRange>
    vim_completion_span(core::Offset origin, std::string_view original) const;
    [[nodiscard]] std::optional<std::string>
    vim_completion_text(const core::LspRange &typed, core::Offset origin,
                        const core::CompletionEntry &entry) const;

    // ---------------------------------------------------------------- 設定の面（ADR 0170・#1016）
    // EditorControllerSettings.cpp の群だけが settings_ を読み書きする（2026-11-18 の追補 2）。
    void accept(const OpenSettings &);
    void accept(const CloseSettings &);
    void accept(const RevertSettings &);
    void accept(const StepSettings &intent);
    void accept(const ChangeSetting &intent);
    void accept(const ChooseSettingsPreset &intent);
    void accept(const FilterSettings &intent);
    void accept(const JumpSettingsSection &intent);
    void accept(const FocusSettingsRow &intent);
    void accept(const ScrollSettings &intent);
    // 入口の 1
    // か所（apply）。面が開いていて、意図が面の意図でも使う人の操作でない意図でもなければ、
    // 写す前に閉じて保存する。
    void close_settings_before(const EditorIntent &intent);
    // 写す前の片付けの 3 つ（設定の面・待ちの鍵・`:s///c` の問い）を順に呼ぶ。
    void settle_before(const EditorIntent &intent);
    // 閉じて、開いたときの写しと違えば 1 回だけ書く（失敗は状態と 1 行の知らせと覚え）。
    void close_settings();
    // 窓を閉じるとき（EndSession）に、面で変えて書けなかった設定をもう 1 回だけ書く。
    void retry_unsaved_settings();
    // 面の切り替えをその場で効かせる（保存しない）。
    void preview_settings(core::EditorSettings settings);
    // 見えている行の鍵（settings_rows の添字の後ろに「言語サーバー」の節の行と畳みの 1
    // 行・#1945）。
    [[nodiscard]] std::vector<std::size_t> visible_settings_rows() const;
    [[nodiscard]] std::optional<std::size_t> focused_settings_row() const;
    // 鍵 key の行の節。
    [[nodiscard]] core::SettingsSection settings_section_of(std::size_t key) const noexcept;

    // ------------------------------------- 設定の「言語サーバー」の節（LSP 段 S4・#1945）
    // EditorControllerLanguageServerRows.cpp の群だけが探しの結果を読み書きする。
    // 表の順の全部の行（表に無い言語の命令の行はその後ろ）。
    [[nodiscard]] std::vector<LanguageServerRowView> language_server_rows() const;
    [[nodiscard]] LanguageServerRowView known_server_row(std::size_t index) const;
    // 見えている行（rows の添字・畳んでいれば畳んでも出す行だけ）。
    [[nodiscard]] std::vector<std::size_t>
    shown_language_server_rows(const std::vector<LanguageServerRowView> &rows) const;
    [[nodiscard]] LanguageServerRowState
    configured_state(std::span<const core::Language> languages) const;
    void describe_configured(LanguageServerRowView &view, const std::string &command) const;
    // 面を開いたときに探しを頼み、合図の道で結果を採る（走っていなければ port を呼ばない）。
    void request_language_server_search();
    void collect_language_server_search();
    // 行 index の「使う」「外す」（rows の数なら「ほかの N 言語を見る」）。
    void act_on_language_server_row(std::size_t index);
    [[nodiscard]] bool used_commands(std::vector<std::pair<std::string, std::string>> &commands,
                                     std::size_t index) const;
    void restart_language_servers_of(core::Language language);
    void unfold_language_server_rows(const std::vector<LanguageServerRowView> &rows);
    // 命令の行が変わった言語の席を止め、失敗の控えを消し、起こすかを決め直す。
    void
    follow_language_server_commands(const std::vector<std::pair<std::string, std::string>> &before,
                                    const std::vector<std::pair<std::string, std::string>> &after);
    // 配達に載せる IME の構え（面が開いている間は閉じ直さない・#1836）。
    [[nodiscard]] ImeStance shown_ime_stance() const;

    // ---------------------------------------------------------------- 起動画面（ADR 0172）
    // EditorControllerStartScreen.cpp の群だけが読み書きする。
    // 最初の frame の合図（決定 3）。以後は起動画面の鍵を受け、出口が起動画面を数え始める。
    void accept(const FirstFramePresented &);
    // 行のクリック（決定 4）。選びの有無に依らず押した行を実行する。
    void accept(const ChooseStartEntry &intent);
    // 起動画面が出る条件（決定 1・追補 2026-11-11）。状態は持たず、毎回アクティブな文書と入力の
    // 状態から決める。
    [[nodiscard]] bool start_screen_shown() const noexcept;
    // 起動画面の選べる行の列（読んだ履歴とブックマーク・「はじめる」の 3 行）。
    [[nodiscard]] std::vector<core::StartEntry> start_screen_entries() const;
    [[nodiscard]] std::optional<StartScreenView> start_screen_view() const;
    // 起動画面の鍵の振り分け（決定 4・追補の 5）。受けた意図は写さない。
    [[nodiscard]] bool route_start_screen(const EditorIntent &intent);
    // 行 1 つの実行（open_listed の 1 本か、操作の頼み）。
    void run_start_entry(const core::StartEntry &entry);
    // 合図の道で届いた読みを採る（決定 3・追補 25）。読みが走っていなければ port を呼ばない。
    void collect_start_lists();
    // 履歴かブックマークを書いた印（追補 25）。次に起動画面が出たときに読み直す。
    void mark_start_lists_stale() noexcept;
    // 履歴から外した path を起動画面の読んだ列から外す（追補 11）。読みが走っていれば覚えて、届いた
    // 列からも外す（読み直しの間に外した行が戻らない・#1495）。
    void drop_start_entry(const core::FilePath &path);
    // 意図の後に、条件が偽なら選びを消し、出ていない → 出ているに変われば読みを頼む（追補の 5・
    // 25）。
    void settle_start_screen();

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
    // INSERT の `Ctrl-o` の命令を待つ間の割り込み（Ctrl+Z・全選択など・ADR 0182 の決定 8）。
    void interrupt_vim_pause();
    void perform(const core::VimUndo &);
    void perform(const core::VimRedo &);
    // `gt` `gT`（ADR 0057 の決定 3）。窓の SwitchTab と同じ accept の 1 本へ写す。
    void perform(const core::VimSwitchTab &effect);
    // `Ctrl-w` の窓の命令（ADR 0148 の決定 7）。2 枠の意図が入るまで何もしない。
    void perform(const core::VimWindowCommand &command);
    void perform(const core::PaneSizing &sizing);
    // 鍵で枠の大きさを変える 1 本（ResizePane・Vim の Ctrl-w・Ex の :resize・ADR 0148 の
    // 2026-11-22 の追補の決定 4）。窓から届いた境界の寸法と今の比から sized_share を呼ぶ。
    void resize_pane(core::PaneSizing sizing);
    // もう 1 つの枠の寸法（追補の決定 5・届く前は操作中の枠の値）。
    [[nodiscard]] const core::TextViewGeometry &other_geometry() const noexcept;
    // もう 1 つの枠を、操作中の枠の形で読む写しにその枠の行数を置いた状態。
    [[nodiscard]] std::optional<EditorState> other_pane_viewed() const;
    // `:s///c` の 1 歩（ADR 0168 の決定 6）。置く当たりを写し、問いを開くか続けるか終える。
    void perform(const core::VimConfirmSubstitute &effect);
    // INSERT にいるあいだの編集は 1 つの undo 単位に吸収する（ADR 0015 の決定 5）。
    [[nodiscard]] core::EditBoundary vim_boundary() const noexcept;

    // lines はマークの行の直し方。矩形の行ごとの置き換え（apply_block だけが渡す）ならマークは
    // 行ごとに直し（marks_block_adjusted・#1361）、VimLineMarks::kept（VimReplaceRange が渡す）なら
    // 行をその場で書き換えた置き換えでマークの行を動かさない（#1360）。
    void replace(const core::OffsetRange &range, std::string_view text, core::EditBoundary boundary,
                 std::variant<core::VimLineMarks, std::span<const core::VimBlockEdit>> lines =
                     core::VimLineMarks::shifted);
    // Vim のモードで eol の旗が立つ文書の置き換えの範囲と本文（ADR 0188 の決定 3 (a)）。空の本文に
    // 本文を足すなら最後の改行も足し、engine を通らない編集が除いた本文を全部消すなら最後の改行も
    // 消す（空で旗が立つ本文にする）。ほかは頼まれたまま。
    [[nodiscard]] std::pair<core::OffsetRange, std::string>
    vim_end_of_line_kept(const core::OffsetRange &range, std::string_view text) const;
    // engine の印が残っていれば（効果が本文を置き換えなかった鍵）、空の単位を積む
    // （ADR 0181 の決定 3）。積む口はこの 1 か所。印は鍵ごとに perform_vim_effect が
    // 置き、replace が編集を積めば消す。restore は効果を写す前のキャレット。
    void push_empty_unit(core::Offset restore);
    // u / Ctrl-r が戻した行の行頭を changed_* の位置に足す（#1374）。
    void note_vim_undone(const core::Edit &edit);
    // 鍵の効果を写す 1 か所（stepped_vim から）。鍵の印を置いてから写し、残った印を空の単位にする。
    void perform_vim_effect(const core::VimStep &step);
    // 全部の行を消した鍵（ADR 0188 の決定 3 (a)）。eol の文書なら最後の改行も消して空の本文にする。
    void remove_every_line(const core::VimStep &step);
    void move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring,
                       core::CaretAffinity affinity = core::CaretAffinity::leading);
    void accept(const PointText &intent);
    // 続けて押した 2 回目と 3 回目（#1250）。pointed_character は押した点の下の文字の先頭
    // （キャレットの境目でなく、点が右半分にあっても指した文字）。select_clicked は通常モードなら語
    // （Vim の `iw` と同じ範囲）か行（改行まで）を選び、Vim は engine の押しの入口
    // （vim_pointer_selected・鍵ではない）で VISUAL を直に置く。
    [[nodiscard]] std::expected<core::Offset, TextLayoutFailure>
    pointed_character(VisualTextView &view, core::TextCaret caret, float x);
    void select_clicked(core::Offset pointed, PointerClicks clicks);
    void select_clicked_in_vim(core::Offset pointed, PointerClicks clicks);
    // origin は移動の起点（キャレットか、通常モードで選択があるときの選択の端・#1393）。
    [[nodiscard]] bool move_visual(const MoveCaret &intent, core::Offset origin);
    [[nodiscard]] std::expected<core::TextLocation, TextLayoutFailure>
    origin_location(VisualTextView &view, const core::TextLocation &at_caret,
                    core::Offset origin) const;
    // 起点の画面行の側（選択を描いていた側・#1393）。← → の畳みと上下の起点が共用する。
    [[nodiscard]] core::CaretAffinity origin_affinity(core::Offset origin) const noexcept;
    // 上下の移動が本文の最初か最後の画面行で止まったときの着地（本文の先頭か末尾・#1386）。
    void move_to_text_edge(VisualTextView &view, const MoveCaret &intent, std::int64_t rows);
    // PgDn / PgUp の画面の先頭の送り。文字組みに失敗したら false。
    [[nodiscard]] bool page_top_shifted(VisualTextView &view, std::int64_t rows);
    // 画面を動かさずにキャレットを見えている範囲へ入れた位置（#785）。
    [[nodiscard]] std::expected<core::TextCaret, TextLayoutFailure> caret_in_view();
    [[nodiscard]] VisualTextView visual_view();
    void clamp_scroll();
    // もう 1 つの枠の先頭の行を、その枠の寸法で clamp_scroll と同じ規則で収める（境界が動いたとき・
    // ADR 0148 の 2026-11-22 の追補の決定 5）。
    void clamp_other_scroll();
    // NORMAL のキャレットは文字の上に置く。Vim の鍵のあとも、クリックと Ctrl+矢印のあとも、
    // 寄せ方はこの 1 本（決定 5）。通常モードと INSERT では何もしない。
    void settle_vim_caret();
    // 鍵の前のキャレットの行と窓（Vim の先頭を前の鍵から引き継ぐかの入力・#1374）。
    using HeldVimWindow = std::pair<core::LineNumber, ScrollState>;
    core::Offset settle_vim_step(const core::VimStep &step, core::VimMode before,
                                 const std::optional<HeldVimWindow> &held);
    [[nodiscard]] bool still_valid_vim_top(const HeldVimWindow &held) const;
    // 鍵の前の窓を保てたら true（キャレットが見えたまま・横は追う・#1374）。
    [[nodiscard]] bool keep_vim_window(const ScrollState &held);
    // engine に文字組みの値を渡す窓か（wrap か格子で幅がある・VimLayoutView と共用）。
    [[nodiscard]] bool vim_rows_layout() const noexcept;
    // INSERT で打った本文の追従（ScrollFollow::vim_typed・#1363）。鍵の前の保留を覚え、鍵の後に
    // 打った本文（と動かない鍵・INSERT へ入る近い鍵）の追従の印を立てる。
    void note_follow_before_key();
    void mark_typed_follow(core::VimMode before, const core::VimStep &step);
    [[nodiscard]] bool stepped_near(core::VimMode before, core::VimMode after) const;
    // engine を通らない意図の後に、Vim の NORMAL と VISUAL のキャレットが 1 文字の境（NORMAL は行の
    // 最後の文字の上）から外れていたときだけ寄せる。INSERT は寄せない。入口 apply の終わりの 1 か所
    // から呼ぶ（ADR 0053 の追補 #1309）。
    void keep_vim_character(const EditorIntent &intent);
    // 追従は頼むだけで、スクロールを読む直前に 1 回だけ解決する（ADR 0128 の決定 1・2）。
    void follow_caret();
    void follow_position(core::TextPosition position);
    void settle_follow();
    // 保留の追従の 1 段を解決する（typed は打った本文の歩き・#1363）。文字組みの失敗なら偽。
    [[nodiscard]] bool settled_follow_to(core::TextPosition position, bool typed);
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
    // 操作中の枠の frame の元（#1572）。VISUAL から開いた `:` の入力行の間は前の選択を描く。
    [[nodiscard]] PaneSource framed_source(const core::VimPattern *pattern) const;
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
    screen_rows_of(const LineView &shown, const core::TextViewGeometry &geometry) const;
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
    // 診断の件数とキャレットの上の診断の文を frame に載せる（ADR 0161 の決定 4）。文はほかの
    // 知らせ（command_message）が無いときだけで、文を出す frame は案内を消す（知らせと同じ）。
    void add_diagnostics(EditorFrame &result) const;
    // 知らせの欄に出す診断の文（優先の式の 1 か所）。add_diagnostics と diagnostic_message_shown が
    // 読む。
    [[nodiscard]] std::optional<core::DisplayText> shown_diagnostic_message() const;
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
    // 入力行の取消の 1 本（Esc と、文書が替わるとき・空の行の Backspace・F1 の一覧と保存の形の
    // 一覧が開いている行を閉じるとき）。録画に `<Esc>` を積むのは打った Esc の意図
    // accept(CancelCommand) だけ（#1528）。
    void cancel_command_input();
    // 入力行に届いた鍵が打った鍵なら印を立てる（Vim の getcmdline の some_key_typed・#1528 の
    // レビュー M1）。再生（`.`・マクロ）・`map` の右辺・press_vim_keys（:normal!）の鍵は打って
    // いない。印は close_command_input が倒す。
    void note_command_key() noexcept;
    // 今の鍵が打った鍵か（再生・`map` の右辺・:normal! でない・Vim の KeyTyped）。
    [[nodiscard]] bool keys_typed() const noexcept;
    void note_recorded_line();
    // 録画中の `:` の行で打った鍵を録画の書き手 vim_macro_typed へ積む（#1528・ADR 0046 の
    // 追補）。打った鍵（再生でも `map` の右辺でもない）で、開いている入力行が Ex の行のときだけ。
    void record_command_keys(std::span<const core::VimKey> keys);
    [[nodiscard]] bool records_command_line() const noexcept;
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
    // 製品の形（ADR 0178 の追補 #1809）。F1 の一覧と登録の 2 つの操作だけが読む。
    core::Distribution distribution_;
    EditorState state_;
    core::TextViewGeometry view_geometry_{};
    // 窓の client の物理画素（ADR 0150 の決定 1）。0 は寸法が届く前。
    std::int32_t window_width_ = 0;
    std::int32_t window_height_ = 0;
    // エクスプローラーの面の高さに入る行数（ルート名の行を含む・#1125）。0 は寸法が届く前。
    std::size_t explorer_lines_ = 0;
    // 2 枠の境界の寸法と、もう 1 つの枠の寸法と行数（VisibleLines が運ぶ・ADR 0148 の 2026-11-22 の
    // 追補の決定 4・5）。other_lines_ が 0 なら操作中の枠の値を使う。
    std::optional<core::SplitExtent> split_extent_;
    core::TextViewGeometry other_geometry_{};
    std::size_t other_lines_ = 0;
    std::optional<TextLayoutFailure> layout_failure_;
    // 今の意図が描き直しを要るか（EditorDelivery.repaint）。apply の入口で true に戻し、合図
    // （AutosaveTick / PreviewTick）が何も変えなかったときだけ false にする（#813）。
    bool repaint_ = true;
    // 頼まれてまだ解決していない追従の位置。後の頼みが前の頼みを置き換える（ADR 0128 の決定 1）。
    std::optional<core::TextPosition> pending_follow_;
    // 保留の追従が Vim の INSERT で打った本文だけから来たか（ScrollFollow::vim_typed・#1363）。
    // follow_position が偽に戻し、打った鍵の終わりの stepped_vim だけが真にする。
    bool pending_typed_ = false;
    // 打った本文の追従の最後の位置（#1363）。打った後の別の頼み（Esc と `.` の後の寄せ・再生の
    // 中の NORMAL の移動）が保留を置き換えても残り、解決は先にここまで歩いてから保留を 1 回の
    // 追従で解く（Vim は打つ周ごとに update_topline を終えてから次の鍵を読む）。NORMAL の `r<CR>`
    // の edit() が改行を入れる前に先頭を確かめた位置（VimStep.edit_topline・#1374）もここに置く。
    std::optional<core::TextPosition> walked_follow_;
    // Vim が窓を決め直す option（`scrolloff` `wrap` `relativenumber` `hlsearch`）を置いた時点の
    // キャレット（#1432・#1524・Vim 9.1 の実測）。後の鍵の頼みが保留を置き換えても残り、解決は
    // 先にここで新しい設定の追従を 1 回流してから保留を解く（Vim は次の鍵の前に今のキャレットで
    // update_topline を流す）。
    std::optional<core::TextPosition> setting_follow_;
    // 鍵の前の保留の追従が無いか打った本文の追従だったか（note_follow_before_key が置く）。
    bool follow_walks_ = false;
    // 鍵の前のキャレット（INSERT へ入る鍵が下へ 1 行より遠く動いたかを見る・#1363）。
    core::Offset caret_before_key_{0};
    // 行高を変えた設定が頼んだ clamp。新しい幾何が届いた後のスクロールの読みで、追従より先に
    // 1 回だけ解決する（ADR 0128 の決定 8）。
    bool pending_clamp_ = false;
    std::optional<float> wanted_x_;
    // `.` と `@` の再生で流す鍵の列と、それぞれの鍵の入れ子の深さと種類（ADR 0046 の決定 3・4・
    // #1312）。replay_depth_ はいま流している鍵の深さで、再生の外（打った鍵）では空。どれも
    // perform(VimReplay) と queue_replay と replayed_mapped_queue の群だけが書く。
    std::deque<core::VimKey> replay_queue_;
    std::deque<ReplayLevel> replay_levels_;
    // `[count]@a` のまだ積んでいない周（#1341）。queue_replay が置き、next_replay_round が減らし、
    // dropped_replay が捨てる。
    std::vector<ReplayRounds> replay_rounds_;
    // replay_rounds の数（queued_round だけが足す・`.` の 1 周も数える）。
    std::size_t replay_round_count_ = 0;
    // いちばん外の再生で列から流した歩の数（#1351）。perform(VimReplay) が再生の始めに 0 に戻し、
    // replay_step_allowed だけが足す。
    std::size_t replay_steps_ = 0;
    // 流す歩の上限（既定は core::vim_replay_key_limit・limit_replay_keys だけが置き換える）。
    std::size_t replay_key_limit_ = core::vim_replay_key_limit;
    std::optional<std::size_t> replay_depth_;
    // `@` の再生の中で `map` の左辺の途中まで読んだ鍵（#1312）。再生の中だけで持ち、列が尽きたら
    // 待たずに流す。state_ の待ち（打った鍵の分）へ持ち越さない。
    std::vector<core::VimKey> replay_pending_;
    // 再生の外で engine に流す鍵の出どころ（typed か mapped）。typed_unit が表に当たった単位の
    // 間だけ mapped にする。再生の中は replayed が勝つ（measured_vim_step）。
    core::VimKeySource unit_source_ = core::VimKeySource::typed;
    // 言語サーバーが running の窓で打った鍵を流している間だけ立つ（typed_vim_key_with_language が
    // ScopedValue で置く・#1962）。立っている間だけ deliver_vim_key が watched_vim_key を呼ぶ。
    bool typed_watch_ = false;
    // 最後に engine へ流した鍵が打った文字（出どころ typed・map の右辺でも再生でもない）で、
    // INSERT が 1 鍵の引数を待たずに受けたなら、その文字と流す前のキャレット。ほかの鍵を
    // 流したら空（watched_vim_key だけが書き、typed_vim_character が読んで消す）。
    std::optional<std::pair<core::VimCharacter, core::Offset>> typed_insert_;
    // 開いている入力行に打った鍵が 1 つでも届いたか（Vim の some_key_typed）。Ex の行を履歴に
    // 足すのはこれが立っているときだけ（remember(CommandLine)）。note_command_key だけが立て、
    // close_command_input だけが倒す。
    bool command_line_typed_ = false;
    // 表に当たった単位の打った左辺で、右辺の最初の鍵を流した後に録画へ積むもの（#1312）。
    // typed_unit が録画中にだけ置き（ScopedValue で戻す）、recorded_held_lhs が積んで消す。
    std::optional<std::span<const core::VimKey>> held_lhs_;
    // replay_waits の数（replayed_step の Pending だけが足す）。
    std::size_t replay_waits_ = 0;
    // 再生を打ち切った鍵の失敗（Issue #983）。perform(VimReplay) が置き、`.` や `@` を打った鍵の
    // deliver_vim_key が自分の失敗として返して消す（Vim の :normal! は残りの鍵を捨てる）。
    std::optional<core::VimRepeatFailure> replay_failure_;
    // 最後に流した Vim の鍵が窓の先頭を確かめ済みで終えたか（VimNavigate の topline が kept・Vim の
    // VALID_TOPLINE・ADR 0019 の追補 #1232）。stepped_vim が鍵ごとに置き、再生を返した外側の鍵は
    // 中の最後の鍵の値を引き継いで、鍵の終わりの寄せ（追従）を頼まない。perform(VimReplay) が
    // 再生の始めに消す。
    bool vim_top_kept_ = false;
    // engine が Vim の入力行（`:` `/` `?`）を開いた（settle_vim_line が読む・#1549・#1569）。
    bool vim_line_open_ = false;
    // イージーモーションの 1 文字の入力行を開いた（settle_jump_line が読む・確定の外で閉じられたら
    // engine の待ちを捨てる）。
    bool jump_line_open_ = false;
    // 印を出したときの本文の改訂番号・スクロール・タブ・モード（settle_labels が比べる）。
    using LabelsGround = std::tuple<std::size_t, ScrollState, std::size_t, core::EditMode>;
    std::optional<LabelsGround> labels_ground_;
    // 録画中に打った `:` の行を開く前の録画（note_recorded_line が置き、確定・Esc・空の行の
    // Backspace が消し、settle_vim_line が行の捨てられたときに戻す・#1569 の L1）。
    std::optional<core::VimRecordedKeys> recorded_line_;
    // reject を通った断りの数（ran_ex_result が命令の要求の断りを Vim の did_emsg として読む・
    // #1549・まとめ枝 #1563）。
    std::size_t refusals_ = 0;
    // 再生の編集を畳む起点（ADR 0046 の決定 3）。再生の外では空。perform(VimReplay) が置いて
    // 消し、再生の中で文書が替わったら reopen_replayed_unit が入った文書の値へ置き直す。
    std::optional<core::EditHistory> replay_history_;
    // 畳みの起点から後で undo を流したか・その後に保存したか（#1672・merge_replayed_edits が
    // 保存時点を写す after_units_folded の unsure）。起点を置く所と再生の終わりが倒す。
    bool replay_undone_ = false;
    bool replay_save_unsure_ = false;
    // 流している鍵の engine の印（ADR 0181）。stepped_vim が効果を写す間だけ置き、replace が
    // 本文を置き換える編集を積めば消す。残れば push_empty_unit が空の単位を積む。
    std::optional<core::VimChange> vim_change_;
    // engine の効果を写している間か（ADR 0182 の決定 8）。replace はこの間の編集を割り込みと
    // 数えず、外の道（通常モードの意図）の編集だけが INSERT の `Ctrl-o` の印を捨てる。
    bool in_vim_effect_ = false;
    // 流している鍵の効果を controller が果たせたか（`]d` `[d` で跳べた・`gl` で札を出せた・#1939 の
    // レビュー R3 の推奨 3）。engine は診断を知らないので歩は失敗のまま返し、stepped_vim がこれで
    // 成功の歩に読み替える（Neovim の `]d` は跳べれば成功・マクロと map の右辺の残りが流れる）。
    bool vim_effect_landed_ = false;
    // 流している鍵で Vim が changed_* に渡した位置（engine の印 VimStep.change の位置・undo / redo
    // は戻した行の行頭）。stepped_vim が鍵の始めに置き、先頭を前の鍵から引き継ぐかの判定
    // （vim_top_still_valid）だけが読む（#1374・ADR 0181 の決定 6 と追補）。
    std::optional<core::TextPosition> vim_changed_from_;
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
    // 面が全文検索を頼んだ控え（ADR 0167 の決定 1・4・5）。面が `!` の出どころの間だけ pattern を
    // 持ち、意図の出口の settle_project_search が検索語の変化と面の閉じを拾う。
    PaletteProjectSearch project_search_;
    // 探して見る面のプレビューの読みの券と保持（ADR 0189 の決定 4・段 P2）。
    FinderPreviews previews_;
    // 全文検索の券の通し番号（プロセスで 1 つ・検索語が変わるたびに進める）。
    std::uint64_t project_tickets_ = 0;
    // 要求の後で ProjectSearchPort にまだ batch が残っているかもしれない間は真（WorkCompleted で
    // collect してたまりを空にする）。collect が空を返し、今の要求が終わっていれば偽に戻す。頼んで
    // いない間は port を呼ばない（verify_quiet_paths）。
    bool project_draining_ = false;
    // 当たりを開いた後にキャレットを置く先（決定 5）。置いた・開けなかったら消す。
    std::optional<ProjectHitLanding> project_landing_;
    // 面を開いたときに控えた診断の候補の指す所（#1939）。候補の command がこの列の位置。
    std::vector<PaletteDiagnostic> palette_diagnostics_;
    // 最後に届いた参照の答え（#1947・次の答えまで `&` の一覧に出す）と、面を開いたときに控えた
    // 参照の候補の指す所（候補の command がこの列の位置）。
    std::optional<LanguageDefinition> held_references_;
    std::vector<PaletteReference> palette_references_;
    // 最後に届いた記号の答えとその文書（#1955・次の答えまで `%` の一覧に出す）と、面を開いたときに
    // 控えた記号の候補の指す所。
    std::optional<std::pair<core::FilePath, LanguageSymbols>> held_symbols_;
    std::vector<PaletteReference> palette_symbols_;
    // 参照の答えを頼んだ所の 1 行（#1958 の決め 3）。参照の控えと一緒に置いて捨てる。
    std::optional<core::DisplayText> held_references_subject_;
    // 今の一覧で参照・記号の候補を作ったか・`%` で頼み直したか（一覧を開くたびに戻す・#1958）。
    bool references_listed_ = false;
    bool symbols_listed_ = false;
    bool symbols_asked_ = false;
    // 出ている引数の情報の札と、手で頼んだ答えを待っているか（#1959）。
    std::optional<SignatureTip> signature_tip_;
    bool signature_invoked_ = false;
    // 参照と記号の当たりを作った数（試験が読む・#1958 のレビュー R1 の契約）。
    std::size_t listed_hits_made_ = 0;
    // 面の上で押した行の節の経路（決定 4・ADR 0056 の決定 9）。離したときに同じ節かを比べて消す。
    // 「…」の行と行の外を押したときは値なし（離しても何もしない）。
    std::optional<core::FilePath> explorer_pressed_;
    // 直前の 1 回のクリックが効いた節の経路（決定 4 の改め #1388）。ダブルクリックの 2 打目は画面の
    // 行を引き直さずにこの節として扱い、2 回読まず・2 回開閉しない。消すのは次の押し・2 打目・
    // 面のマウス以外の使う人の操作（apply の入口の 1 か所）・面を消す / ルートを替える
    // （sync_explorer）。
    std::optional<core::FilePath> explorer_clicked_;
    // 境をドラッグしている間の面の幅（DIP・ADR 0162 の追補 D78）。設定には離したときにだけ書く。
    std::optional<std::int32_t> explorer_dragged_;
    // 字体の一覧（#597・D58）。補完が求めた最初の 1 回だけ読み、読めなくても再読しない。
    std::optional<core::FontFamilies> font_families_;
    // プレビューの券（ADR 0155 の決定 6）。要求ごとに 1 つ進み、閉じて開き直しても戻さない
    // （閉じる前の要求の結果を開き直した後の券と取り違えない）。
    std::uint64_t preview_tickets_ = 0;
    // プレビューの画像の券（ADR 0155 の追補 50）。画像の要求ごとに 1 つ進み、戻さない。
    std::uint64_t preview_image_tickets_ = 0;
    // 画像の口に頼んで結果をまだ引いていない券（追補 51・高々 1 つ）。文書が替わっても結果を引くま
    // で次を頼まない（ワーカーを待たせるのは最大 1 枚の復号の時間）。
    std::optional<std::uint64_t> preview_image_pending_;
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
    // 全面の間に Vim の `g` を受けて次の鍵を待っているか（`gg` `gt` `gT`・ADR 0155 の改訂 17）。
    // 読み書きは route_full_preview の 1 か所。
    bool preview_vim_g_ = false;
    // 起動画面（ADR 0172）。最初の frame を出したか（以後だけ鍵を受ける）・前の意図の出口で
    // 出ていたか・読みが走っているか（届くまで 1 本）・前の読みの後に履歴かブックマークを書いたか
    // （最初は立てておく・追補 25）・読んだ履歴とブックマーク（次に届くまで持つ）・選んだ行（追補
    // 2026-11-11 の 5・条件が偽になったら消す）。読み書きは EditorControllerStartScreen.cpp の群
    // だけ（印を立てるのは mark_start_lists_stale）。
    bool first_frame_presented_ = false;
    bool start_screen_was_shown_ = false;
    bool start_lists_reading_ = false;
    bool start_lists_stale_ = true;
    std::optional<StartLists> start_lists_;
    // 読みを頼んだ後に履歴から外した path（届いた列から外す・届いたら捨てる・#1495）。
    std::vector<core::FilePath> start_forgotten_;
    std::optional<std::size_t> start_selection_;
    // 開いている右クリックのメニュー（ADR 0078 の追補 #1051 / #1790）。
    std::optional<ContextMenuOpen> context_menu_;
    // LinkOpenPort に頼んで結果をまだ引いていない数（#1869 の段 L2）。
    std::size_t link_opens_pending_ = 0;
    // 開いている補完の面と、面の通し番号（ADR 0190 の決定 1・5）。
    std::optional<CompletionSession> completion_;
    std::uint64_t completion_sessions_ = 0;
    // 開いている設定の面（ADR 0170 の 2026-11-18 の追補 2）。
    std::optional<SettingsOpen> settings_;
    // 言語サーバーの探し（#1945）。頼んで結果をまだ採っていないか・表の行ごとに最初に見つかった
    // 候補（候補の添字と見つかった物・値なしは見つからない）。まだ 1 度も届いていなければ値なし。
    bool language_server_search_reading_ = false;
    std::optional<std::vector<std::optional<std::pair<std::size_t, FoundLanguageServer>>>>
        found_language_servers_;
    // 面を閉じたときの書き込みが失敗し、状態の設定がまだファイルに無い（R1）。書けたとき・窓を
    // 閉じるときの書き直しの後に落とす。
    bool settings_unsaved_ = false;
    // いまの意図の入口（close_settings_before）で面を閉じて書けなかった。同じ意図の中の
    // retry_unsaved_settings を飛ばす（書き込みは 1 回だけ・#1835 の再確認 R-b1）。
    bool settings_write_failed_now_ = false;
    // いまの意図の入口で面を閉じた（面が開いていた）。同じ意図は補完の面も hover の札も出さず、
    // 定義の問いも出さない（面どうしの排他・ADR 0190 の決定 5・6 と ADR 0170）。
    bool settings_closed_now_ = false;
    // 失敗がまだ届きうるエクスプローラーで場所を開く頼みの数（#1790・collect の呼び分け）。
    std::size_t reveals_pending_ = 0;
    // 写している途中の意図の深さと、いちばん外の意図の入口の印（ADR 0137 の追補 #1625）。apply の
    // 入口と出口が整数を書くだけで、例外で抜けると深さは戻らない（落ちたときは「意図の中」と読む）。
    std::size_t intent_depth_ = 0;
    IntentMark intent_mark_;
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
    // ミニマップの窓の行の塊（ADR 0171 の決定 4）と設定の値（決定 1）。minimap_shown_ を置くのは
    // sync_minimap だけ。
    MinimapCache minimap_;
    bool minimap_shown_ = false;
    // 最後の frame の見えている論理行の数（add_minimap が面の範囲に使った値・決定 5 の追補）と、
    // 面を押したときのその値（ドラッグの間は押したときの値で送り、折り返しで見えている行の数が
    // 変わってもドラッグを単調に保つ）。0 は frame がまだ無い。
    std::size_t minimap_visible_ = 0;
    std::size_t minimap_grab_visible_ = 0;
    // 押したタブと、最後に面が送った位置（先頭の行と画面行・レビュー V-1476 の M-2 と L-6）。
    std::optional<AutosaveId> minimap_grab_tab_;
    std::optional<MinimapAt> minimap_sent_;
    // 言語サーバーの席ごとのつながり（ADR 0160 の決定 5・6・2026-11-20 の追補）。
    // EditorControllerLanguage*.cpp の群だけが読み書きする。
    LanguageServers language_servers_;
    // Vim の `map` の表（ADR 0176 の決定 2）。設定から refresh_key_map だけが作る。
    core::KeyMap key_map_;
    // この意図が頼む待ちのタイマー（EditorDelivery.map_timer）。apply の入口で空に戻し、待ちを
    // 積む・流す所だけが置く。
    std::optional<MapTimer> map_timer_;
    // 窓が非アクティブか（WindowDeactivated で真・使う人の操作の意図で偽）。非アクティブの間に
    // 届いた hover の答えは札にしない（#1843 のレビュー R3）。
    bool window_inactive_ = false;
    // 定義の問いを出したときの選択の起点（答えが届いたときに同じなら跳ぶ・R1）。
    std::optional<core::Offset> definition_anchor_;
    // この意図が頼むタブの札の待ちのタイマー（EditorDelivery.tab_tip_timer・#1789）。apply の
    // 入口で空に戻し、札の段を変える所（帯の hover・合図・入口の消す 1 か所）だけが置く。
    std::optional<TabTipTimer> tab_tip_timer_;
    // 最後に届いたキャレットの動きの進み（ADR 0192 の決定 3）。CaretMotionTick だけが置き、
    // frame が写す。
    core::CaretMotionClock caret_motion_clock_{};
};
} // namespace nenenib::application
