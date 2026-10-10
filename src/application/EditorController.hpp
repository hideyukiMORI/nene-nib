#pragma once

#include "AppearancePort.hpp"
#include "ClipboardPort.hpp"
#include "CodePagePort.hpp"
#include "CommandChoice.hpp"
#include "CompositionView.hpp"
#include "DetectedText.hpp"
#include "Document.hpp"
#include "DocumentView.hpp"
#include "EditBoundary.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "ExDocumentRequest.hpp"
#include "ExPaletteRequest.hpp"
#include "ExTabRequest.hpp"
#include "FileBookmarks.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "LineView.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PaletteFolder.hpp"
#include "PaletteOrigin.hpp"
#include "ScrollState.hpp"
#include "SelectionAnchoring.hpp"
#include "Session.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TitleBarInput.hpp"
#include "UnloadedDocument.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimCharacter.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimPattern.hpp"
#include "VimRegister.hpp"
#include "VimRepeatFailure.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
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
    [[nodiscard]] EditorFrame frame() const;
    [[nodiscard]] bool command_line_active() const noexcept;
    [[nodiscard]] bool command_palette_active() const noexcept;
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
    void load_vim_clipboard();
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
    // 開く道の 1 本（ADR 0056 の決定 5・ADR 0060 の決定 7）。同じファイルのタブへ切り替える →
    // 何も書いていない無題に開く → 新しいタブ。開けなければタブを足さず失敗を返す。
    [[nodiscard]] std::expected<void, FileFailure> open_document(const core::FilePath &path);
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
    // 帯（ADR 0056 の決定 2・3・8）。帯の幅・ホイール・マウスを載せた要素。
    void accept(const TitleBarWidth &intent);
    void accept(const ScrollTabs &intent);
    void accept(const PointTitleBar &intent);
    // 帯の配置の入力（96 DPI・幅は DIP・決定 8）と、送り量をアクティブなタブが見える所へ直す
    // 1 本（切り替え・新しいタブ・閉じる・開く・帯の幅のたび）。帯の幅がまだ 0 なら送り量は 0。
    [[nodiscard]] core::TitleBarInput title_bar_input() const;
    void reveal_active_tab();
    // 開く（決定 5）の 3 つの枝の材料。同じファイルを開いているタブの帯の位置・何も書いていない
    // 無題か・読んで復号した本文と文書。
    [[nodiscard]] std::optional<std::size_t> open_tab_of(const core::FilePath &path) const;
    [[nodiscard]] bool blank_untitled() const;
    [[nodiscard]] std::expected<std::pair<core::TextBuffer, Document>, FileFailure>
    read_document(const core::FilePath &path);
    // 出ていく前に文書の上の一時の値を閉じる（入力行・検索の preview・IME の変換中の文字列）。
    void leave_document();
    // 入ってきた文書のスクロールを今の窓の行数で収め、NORMAL のキャレットを文字の上へ寄せる。
    void enter_document();
    // まだ読んでいない文書を読む 1 本（ADR 0059 の決定 5）。帯の位置 position が読み込み済みか
    // アクティブなら真。まだ読んでいない文書なら読んで束に置き換えて真、読めなければ帯から外して
    // 1 行知らせて偽。範囲の外は偽（何もしない）。タブが切り替わる入口（SwitchTab・WalkRecentTab・
    // CloseTab の後の隣・起動）は状態を動かす前にこれを呼ぶ。
    [[nodiscard]] bool reach_tab(std::size_t position);
    // アクティブを閉じて隣へ移る（隣を reach_tab で読む・1 本も残らなければ空の無題）。
    void close_active_tab();
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
    void submit_palette(const core::CommandPalette &palette);
    void evaluate_command(std::string_view text);
    // Ex のタブの命令の写し先（ADR 0057 の決定 5・6）。命令が増えたら switch が落ちる（CPP-002）。
    void run_tab_request(const core::ExTabRequest &request);
    // Ex のファイル命令も Ctrl+P と同じ面へ渡す（ADR 0064）。
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
    // frame の知らせ（ADR 0062 の決定 17）。状態の知らせが先で、無ければ面が開いていて今の券が
    // 打ち切られたときの 1 行。判断はここだけ（ARC-001）。
    [[nodiscard]] std::optional<core::DisplayText> command_message() const;
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
    void apply_block(const std::vector<core::VimBlockEdit> &edits, core::Offset caret);
    // `.` の再生（ADR 0030 の決定 7）。鍵を同じ accept の経路へ流すだけで、`.` は記録されない
    // ので再帰は深さ 1 で止まる。UI・IME・描画は通らない。
    void perform(const core::VimReplay &effect);
    void interrupt_vim_insert();
    void perform(const core::VimUndo &);
    void perform(const core::VimRedo &);
    // `gt` `gT`（ADR 0057 の決定 3）。窓の SwitchTab と同じ accept の 1 本へ写す。
    void perform(const core::VimSwitchTab &effect);
    // INSERT にいるあいだの編集は 1 つの undo 単位に吸収する（ADR 0015 の決定 5）。
    [[nodiscard]] core::EditBoundary vim_boundary() const noexcept;

    void replace(const core::OffsetRange &range, std::string_view text,
                 core::EditBoundary boundary);
    void move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring);
    // NORMAL のキャレットは文字の上に置く。Vim の鍵のあとも、クリックと Ctrl+矢印のあとも、
    // 寄せ方はこの 1 本（決定 5）。通常モードと INSERT では何もしない。
    void settle_vim_caret();
    void follow_caret();
    void follow_position(core::TextPosition position);
    // incsearch の preview（ADR 0041 の決定 3・5）。
    void update_search_preview();
    void restore_search_origin(const ScrollState &origin);
    void undo_edit();
    void redo_edit();
    void copy_selection();
    void cut_selection();
    void paste_clipboard();
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
    [[nodiscard]] LineView line_view(core::LineNumber line, const core::OffsetRange &range,
                                     const core::VimPattern *pattern) const;
    [[nodiscard]] std::vector<LineView> visible_lines() const;
    // 本文の変換（面が開いていれば空）と面の入力行の変換（面が開いているときだけ）。どちらか
    // 一方にだけ載る（ADR 0061 の決定 4）。
    [[nodiscard]] std::optional<CompositionView> composed() const;
    [[nodiscard]] std::optional<CompositionView> command_composed() const;
    // 録画中のマクロの名前。通常モードでは録画が止まっているので出さない（ADR 0046 の決定 8）。
    [[nodiscard]] std::optional<char> recording_name() const;
    [[nodiscard]] core::GuideContext guide_context() const noexcept;
    // Vim の NORMAL では IME を切ってあるので変換は来ないはずだが、来たら捨てる（決定 4）。
    // Ex の行と検索の行でも捨て、面が開いていれば受ける（ADR 0061 の決定 3）。
    [[nodiscard]] bool composition_ignored() const noexcept;
    // 入力行を閉じる 1 本。面を閉じるときに残っている変換は消す（ADR 0061 の決定 3）。
    void close_command_input();
    // 確定した文字列を Vim の打鍵として流す。`.` の再生とマクロが後で自然に載る（決定 4）。
    void type_as_vim_keys(std::string_view utf8);
    void fail(FileFailure failure);
    // 判定済みの列を本文へ渡す。CP932 の変換だけはポートの向こう（ADR 0080）。
    [[nodiscard]] std::expected<core::TextBuffer, FileFailure> decoded(core::DetectedText text);
    [[nodiscard]] std::expected<std::string, FileFailure> encoded(core::TextEncoding encoding,
                                                                  std::string utf8);

    EditorPorts ports_;
    EditorState state_;
    // `.` と `@` の再生で流す鍵の列と、それぞれの鍵の入れ子の深さ（ADR 0046 の決定 3・4）。
    // replay_depth_ はいま流している鍵の深さで、再生の外（打った鍵）では空。どれも
    // perform(VimReplay) と queue_nested_replay だけが書く。
    std::deque<core::VimKey> replay_queue_;
    std::deque<std::size_t> replay_depths_;
    std::optional<std::size_t> replay_depth_;
    // 再生の編集を畳む起点（ADR 0046 の決定 3）。再生の外では空。perform(VimReplay) が置いて
    // 消し、再生の中で文書が替わったら reopen_replayed_unit が入った文書の値へ置き直す。
    std::optional<core::EditHistory> replay_history_;
    std::optional<core::TextBuffer> replay_text_;
    // 同じ意図の中で読めなかったタブの数（ADR 0059 の「知らせの文言」）。begin_intent が 0 に戻す。
    std::size_t unreached_tabs_ = 0;
    // 面が同じフォルダを頼んだ控え（ADR 0062 の決定 14・15）。券は面を開くたびに進み、ほかは
    // 面を開くときに置き直す。面が閉じている間は読まない。
    PaletteFolder palette_folder_;
};
} // namespace nenenib::application
