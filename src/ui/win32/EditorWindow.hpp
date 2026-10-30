#pragma once

#include "Appearance.hpp"
#include "BodyLayout.hpp"
#include "CommandEdit.hpp"
#include "Composition.hpp"
#include "CoverWindow.hpp"
#include "Direct2DRenderer.hpp"
#include "EditMode.hpp"
#include "EditorController.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "EndSession.hpp"
#include "ExitCause.hpp"
#include "ExplorerLayout.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "FontSizeAdjustment.hpp"
#include "HistoryDirection.hpp"
#include "ImeOpenState.hpp"
#include "ImeStance.hpp"
#include "InputLinePrompt.hpp"
#include "Milestone.hpp"
#include "PaneLayouts.hpp"
#include "PresentedMilestones.hpp"
#include "PreviewTaskPress.hpp"
#include "RenderFailure.hpp"
#include "RendererSources.hpp"
#include "RgbColor.hpp"
#include "SessionEnd.hpp"
#include "SplitLayout.hpp"
#include "StatusBarHit.hpp"
#include "TabCommand.hpp"
#include "TimingPort.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "VimMode.hpp"
#include "WheelTurn.hpp"
#include "WindowFailure.hpp"
#include "WindowTimer.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
// 枠なしの編集窓。操作は意図として controller へ渡し、描画は EditorFrame を写すだけ（ARC-011 /
// CPP-017）。タイトルバーとステータスバーの位置は core のレイアウト純関数が決める（ADR 0008）。
// 描画は WM_PAINT の 1 か所に集める。意図を適用したら無効化するだけで、まとめて来た入力は
// 1 フレームで描かれる（ADR 0011 の決定 6）。節目は打つだけで、時刻は知らない（決定 1）。
class EditorWindow final
{
  public:
    // sources の device の箱は worker が先に作り、最初の renderer が受け取る（ADR 0131）。
    [[nodiscard]] static std::expected<std::unique_ptr<EditorWindow>, WindowFailure>
    create(HINSTANCE instance, application::EditorController &controller,
           application::TimingPort &timing, RendererSources sources);
    ~EditorWindow();
    EditorWindow(const EditorWindow &) = delete;
    EditorWindow(EditorWindow &&) = delete;
    EditorWindow &operator=(const EditorWindow &) = delete;
    EditorWindow &operator=(EditorWindow &&) = delete;

    [[nodiscard]] bool rendering_failed() const noexcept;
    // クリップボードの OpenClipboard に渡す所有者。合成ルートが adapters へ結ぶためだけの口。
    [[nodiscard]] HWND handle() const noexcept;
    // 裏の仕事の「届いた」の合図（ADR 0062 の決定 7）。合成ルートが adapters へ結ぶためだけの口。
    // 返す値は窓のハンドルを値で持ち、窓へ合図のメッセージを post するだけで、窓の状態に触れない
    // （ワーカーのスレッドから呼ばれる・窓が壊れた後に呼ばれても post が失敗するだけ）。
    [[nodiscard]] std::function<void()> work_signal() const;
    // 最初の frame を WARP で子窓に描いていて、本物の device を待っている間だけ真
    // （ADR 0132 の決定 4・合成ルートのメッセージループが読む）。
    [[nodiscard]] bool provisional() const noexcept;
    // 箱の event B が立ったら合成ルートが呼ぶ。仮の renderer のときだけ、本窓に本物の device で
    // renderer を作って同じ frame を出し、仮の renderer を解放して子窓を壊す（決定 3）。
    void adopt_device();

  private:
    EditorWindow(HINSTANCE instance, application::EditorController &controller,
                 application::TimingPort &timing, RendererSources sources);
    [[nodiscard]] std::expected<void, WindowFailure> initialize();
    [[nodiscard]] std::expected<void, WindowFailure> start_rendering();
    // 本物の device がまだなら子窓に仮の renderer を、
    // そうでなければ（仮が作れなくても）本窓に作る。
    [[nodiscard]] std::expected<Direct2DRenderer, RenderFailure> create_renderer();
    [[nodiscard]] std::expected<Direct2DRenderer, RenderFailure> create_provisional_renderer();
    void apply_backdrop(const application::EditorDelivery &frame);
    [[nodiscard]] application::EditorDelivery follow_look(application::EditorDelivery delivery);
    void place_at_screen_centre();
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM word, LPARAM data) noexcept;
    LRESULT dispatch(UINT message, WPARAM word, LPARAM data) noexcept;
    [[nodiscard]] LRESULT calculate_client(WPARAM word, LPARAM data) noexcept;
    [[nodiscard]] LRESULT hit_test(LPARAM data) noexcept;
    [[nodiscard]] LRESULT frame_message(UINT message, WPARAM word, LPARAM data);
    [[nodiscard]] LRESULT pointer_message(UINT message, WPARAM word, LPARAM data);
    [[nodiscard]] LRESULT key_message(UINT message, WPARAM word, LPARAM data);
    // 窓を閉じる・OS の終了・壊れた・裏の仕事の合図の 4 通。
    [[nodiscard]] LRESULT lifetime_message(UINT message, WPARAM word, LPARAM data);
    // Ctrl+Tab の歩きの確定（ADR 0058 の決定 4）。歩いているときだけ SettleRecentTab を送る。
    void settle_tab_walk();
    void limit_size(LPARAM data) const noexcept;
    // 帯の配置は状態の 4 つの値と窓の幅から毎回作る。表示値は作らない（ADR 0056 の決定 9）。
    [[nodiscard]] core::TitleBarLayout title_bar() const;
    [[nodiscard]] core::TitleBarTarget title_bar_target_at(LPARAM data) const;
    [[nodiscard]] std::int32_t title_bar_width() const;
    // 帯の上の押下を扱ったら true（本文とステータスバーへは流さない）。
    [[nodiscard]] bool click_title_bar(LPARAM data);
    void release_title_bar(LPARAM data);
    void press_middle(LPARAM data);
    void middle_click(LPARAM data);
    void point_at(LPARAM data);
    void hover(const std::optional<core::TitleBarTarget> &target);
    // タブを閉じる流れ（未保存なら切り替えて確かめる・ADR 0056 の決定 6）。
    void close_tab(std::size_t tab);
    void finish_tab_action(std::optional<std::size_t> close_request, bool closing);
    void finish_operation(std::optional<core::EditorOperation> operation_request);
    // 開いた文書に格子表示を聞く（ADR 0140 の決定 2）。deliver の終わりと、起動の終わり
    // （最初の frame の後）から呼ぶ。
    void ask_display();
    void finish_display_question(bool question);
    [[nodiscard]] LRESULT press_caption(UINT message, WPARAM word, LPARAM data) noexcept;
    void activate_caption(WPARAM word) noexcept;
    void click_client(LPARAM data);
    void click_palette(LPARAM data);
    void place_caret(LPARAM data);
    void click_body(LPARAM data);
    void point_text(POINT point, core::SelectionAnchoring anchoring);
    void begin_selection_drag();
    void end_selection_drag();
    void drag_selection(LPARAM data);
    void scroll_selection();
    // プレビューの枠のチェックボックス（ADR 0155 の追補 24）。押した要素を覚え、離した要素が同じ
    // ときだけ TogglePreviewTask を送る（ADR 0056 の決定 9 の形）。
    void press_preview_task(LPARAM data);
    void release_preview_task(LPARAM data);
    // 描く唯一の口。Present が返った直後に frame_presented を打ち、本文を描いた最初の frame では
    // body_presented も打つ（ADR 0011 の決定 1・ADR 0138 の決定 2）。
    [[nodiscard]] std::expected<void, RenderFailure> draw_frame();
    void paint();
    // 意図の適用で変わった見た目は、次の WM_PAINT でまとめて描く（決定 6）。
    void invalidate() noexcept;
    void present();
    // 描画を 1 回だけ作り直す（ADR 0133 の決定 2）。古い renderer を先に解放してから作る。
    // 描画の失敗の 4 経路（present・サイズ・DPI・文字組み）がここを通ってから abandon へ行く。
    [[nodiscard]] bool recover();
    // 文字組みに失敗した delivery の後も描けるか。作り直せなければ abandon して false。
    [[nodiscard]] bool survives_layout(const application::EditorDelivery &delivery);
    // 描画を失った終わり（ADR 0133 の決定 1〜3）。renderer を捨て、未保存のタブを取消の無い確認に
    // 通してから leave(rendering_lost) する。窓を直接は壊さない。
    void abandon();
    void refresh_appearance();
    void resize();
    void change_dpi(WPARAM word, LPARAM data);
    // 意図を送る唯一の口。入れ子の深さを数え、途中で届いた裏の仕事の合図は、いちばん外の send が
    // 終わる所で 1 回だけ WorkCompleted にして送る（ADR 0062 の決定 7）。
    void send(const application::EditorIntent &intent);
    // send の本体（意図を写して frame を窓へ映す）。深さは数えない。
    void deliver(const application::EditorIntent &intent);
    // 合図のメッセージを受けた。send の途中なら覚えるだけ、そうでなければ送る。
    void receive_work();
    // WM_TIMER を窓のタイマーへ写す。自動退避の合図は AutosaveTick を 1 回送る（ADR 0137
    // の決定 1）。
    void receive_timer(WPARAM word);
    void start_signal_timers();
    void follow_preview_timer();
    void stop_preview_timer();
    [[nodiscard]] bool previewing() const;
    // 起動の問い（ADR 0137 の決定 6）。最初の frame の後に自分へ post した合図で 1 回だけ呼ばれる。
    // 前回の退避が残っていれば「はい / いいえ」を聞いて、答えの意図を送る。
    void offer_autosave();
    void type_character(WPARAM word);
    void type_text(std::string utf8);
    void press_key(WPARAM word);
    [[nodiscard]] bool press_bookmark_key(WPARAM word, LPARAM data);
    // F1（操作の一覧・ADR 0078 の決定 9）と F5（日時の挿入・ADR 0175）。扱ったら true。
    [[nodiscard]] bool press_function_key(WPARAM word, LPARAM data);
    // 通常 / Vim の切り替え（Ctrl+Shift+Space・D61）。反復では切り替えない。扱ったら true。
    [[nodiscard]] bool press_mode_key(WPARAM word, LPARAM data);
    // 鍵と一覧が実行する唯一の口（ADR 0078 の決定 7）。
    void run_operation(core::EditorOperation operation);
    void walk_recent_tab(core::TabCommand command);
    void adjust_font(core::FontSizeAdjustment adjustment);
    void toggle_mode();
    // タブの鍵（ADR 0056 の決定 10）。扱ったら true。
    [[nodiscard]] bool press_tab_key(WPARAM word);
    void run_tab_command(core::TabCommand command);
    void press_command_key(WPARAM word);
    void press_command_control_key(WPARAM word);
    [[nodiscard]] bool press_palette_key(WPARAM word);
    void send_palette_or(core::CommandEdit edit,
                         const std::optional<application::EditorIntent> &otherwise);
    void press_plain_key(WPARAM word);
    void press_vim_key(WPARAM word);
    void press_control_key(WPARAM word);
    // IMM32（ADR 0014）。WM_IME_STARTCOMPOSITION / WM_IME_COMPOSITION は DefWindowProcW へ
    // 渡さないので、IME の既定の変換窓は出ず、確定文字も WM_CHAR には流れない（決定 3）。
    [[nodiscard]] LRESULT compose_message(UINT message, WPARAM word, LPARAM data);
    void compose(LPARAM data);
    void send_composition(const core::Composition &composition);
    void end_composition();
    void place_candidate_window();
    // frame の構え（ImeStance）と入力行の種類の変わり目を実行する。決めるのは application
    // （ADR 0061 の決定 2）。
    void follow_ime(const application::EditorDelivery &frame);
    void close_ime();
    void restore_ime();
    // IME の側に残った変換を取り消す（ImmNotifyIME の CPS_CANCEL・ADR 0061 の決定 3）。
    void cancel_ime_composition();
    // Ctrl+Z / Ctrl+Y は Vim では u / Ctrl-r に譲り、Ctrl+R は Vim のときだけ意味を持つ。
    void send_history(core::HistoryDirection direction);
    void send_vim_redo();
    void open_document();
    void open_dropped_files(WPARAM word);
    void save_document();
    void save_document_as();
    void close_window();
    // 終わり方の表（application の exit_plan）に従って確かめ、通り抜けたときだけ leave する
    // （ADR 0133 の決定 2）。close_window と abandon が通る。
    void end_window(application::ExitCause cause, application::SessionEnd reason);
    // 未保存のタブを帯の左から順に映して confirm で確かめる（#237）。止められたら false。
    [[nodiscard]] bool confirm_unsaved_tabs(bool (EditorWindow::*confirm)());
    // 窓を壊す唯一の口（ADR 0059 の決定 3・ADR 0133 の決定 1）。前回のタブの一覧を書いてから壊す。
    // end_window と、最後の 1 つを閉じる close_tab が通る。デストラクタは作りかけの窓の後始末だけ。
    void leave(application::SessionEnd reason);
    // OS の終了を問われたら exit_plan(session_end) に従って止めるか通すかを返す（ADR 0133）。
    // end_window を通らず、leave_now でも窓を壊さない（確定の WM_ENDSESSION は一覧を書くだけ）。
    [[nodiscard]] LRESULT query_session_end();
    // OS の終了を止めた理由を、未保存が無くなったら外す（ADR 0133 の決定 4）。
    // deliver の末尾と leave_now の答えが呼ぶ。
    void release_session_block();
    // 前回のタブの一覧を書く意図を送る（leave と WM_ENDSESSION）。この後は窓へ描かない。
    void remember_session(const application::EndSession &intent);
    // 未保存なら聞く。閉じる・開き直すのを続けてよいときだけ true（ADR 0010 の決定 10）。
    [[nodiscard]] bool confirm_discard();
    // 描画を失った後の「保存しますか」（ADR 0133 の決定 3）。取消は無く、保存するまでか捨てるまで
    // 聞き直す。いつも true。
    [[nodiscard]] bool confirm_lost_tab();
    void update_title(const application::EditorDelivery &frame);
    void announce(const application::EditorDelivery &frame);
    void announce_settings(const application::EditorDelivery &frame);
    void offer_utf8(const core::FilePath &path);
    void turn_wheel(WPARAM word, LPARAM data);
    [[nodiscard]] bool over_title_bar(LPARAM data) const;
    [[nodiscard]] bool over_other_pane(LPARAM data) const;
    void follow_split(core::SplitLayout previous, application::EditorDelivery &delivery);
    void follow_explorer(application::EditorDelivery &delivery);
    void choose_explorer_root();
    // エクスプローラーの面の群（EditorWindowExplorer.cpp・ADR 0162 の決定 4）。
    void scroll_explorer(std::int32_t delta);
    [[nodiscard]] bool over_explorer(LPARAM data) const;
    void press_explorer_key(WPARAM word);
    [[nodiscard]] std::optional<std::size_t> explorer_row_at(LPARAM data) const;
    [[nodiscard]] bool press_explorer(LPARAM data);
    void release_explorer(LPARAM data);
    [[nodiscard]] bool double_click_explorer(LPARAM data);
    void run_tab_operation(core::EditorOperation operation);
    void run_pane_operation(core::EditorOperation operation);
    void run_edit_operation(core::EditorOperation operation);
    // 意図を 1 つ送るだけで、何をするかは controller が決める操作（日時の挿入・印刷）。
    void run_document_operation(core::EditorOperation operation);
    [[nodiscard]] bool keeps_block_key(core::EditorOperation operation) const noexcept;
    void scroll_tabs(std::int32_t delta);
    void zoom_wheel(std::int32_t delta);
    void scroll_palette(std::int32_t delta);
    void scroll_body(std::int32_t delta, LPARAM data);
    void read_wheel_setting();
    [[nodiscard]] application::VisibleLines body_view() const;
    [[nodiscard]] core::PaneLayouts body_panes(const core::EditorSettings &settings) const;
    [[nodiscard]] core::ExplorerLayout explorer_frame(const core::EditorSettings &settings) const;
    [[nodiscard]] core::BodyLayout active_body(const core::EditorSettings &settings) const;

    HINSTANCE instance_;
    application::EditorController &controller_;
    application::TimingPort &timing_;
    BodyTextLayouts &text_layouts_;
    DeviceWarmup &warmup_;
    // present が返った frame の節目（frame_presented と本文の最初の body_presented・#546）。
    PresentedMilestones presented_;
    // 題名は変わったときだけ OS へ渡す。毎フレーム SetWindowTextW を呼ばない（決定 13）。
    std::wstring window_title_;
    // WM_CHAR は UTF-16 の 1 単位ずつ来るので、サロゲートの上位を次の下位まで預かる（ADR 0009）。
    wchar_t pending_high_surrogate_ = 0;
    // 前の配達で窓が当てた字（大きさと字体）と明暗。配達のたびに比べ、変わったときだけ当てる
    // （ADR 0150 の決定 5）。
    std::pair<float, std::string> shown_font_;
    core::Appearance shown_appearance_ = core::Appearance::light;
    // 前の配達で窓の縁に当てた帯の地（#795）。配色が変われば明暗が同じでも当て直す。
    core::RgbColor shown_border_{};
    std::int32_t zoom_wheel_remainder_ = 0;
    std::int32_t tab_wheel_remainder_ = 0;
    // 本文のホイールの余り（「delta × 1 刻みの行数」の単位・WheelTurn）と、OS の
    // SPI_GETWHEELSCROLLLINES の値（起動と WM_SETTINGCHANGE で読む・#499）。
    std::int32_t body_wheel_remainder_ = 0;
    // 面の一覧のホイールの余り（単位は本文と同じ・#737）。本文の余りとは別の対象なので共有しない。
    std::int32_t palette_wheel_remainder_ = 0;
    // エクスプローラーの面のホイールの余り（ADR 0162 の決定 4）。
    std::int32_t explorer_wheel_remainder_ = 0;
    // 面の上で左を押して、まだ離していない（離したときにだけ PointExplorer の released を送る）。
    bool explorer_pressed_ = false;
    UINT wheel_setting_ = default_wheel_scroll_lines;
    // 帯で押した要素（左ボタンと中ボタンで 1 つずつ）。離した要素と同じときだけ動かし、離したら
    // 消す（ADR 0056 の決定 9）。
    std::optional<core::TitleBarTarget> left_pressed_;
    std::optional<core::TitleBarTarget> middle_pressed_;
    // プレビューの枠で押したチェックボックス（離したとき・次に押したときに消す）。
    PreviewTaskPress preview_task_press_;
    // OSの捕捉の寿命だけ。選択のanchor/caretの正本はcontrollerにある。
    bool selecting_ = false;
    POINT selection_point_{};
    static constexpr UINT_PTR selection_timer = std::to_underlying(WindowTimer::selection);
    static constexpr UINT_PTR autosave_timer = std::to_underlying(WindowTimer::autosave);
    static constexpr UINT_PTR preview_timer = std::to_underlying(WindowTimer::preview);
    static constexpr UINT_PTR language_server_timer =
        std::to_underlying(WindowTimer::language_server);
    // プレビューの合図のタイマーを張っているか。開いているかの正本は application の枠の状態。
    bool preview_ticking_ = false;
    // 最後に本文の寸法を送ったときにエクスプローラーの面が出ていたか（ADR 0162 の決定 2）。
    bool shown_explorer_ = false;
    // いまの編集モード。鍵をどちらの表で引くかを決めるだけで、正本は EditorState（ARC-004）。
    core::EditMode mode_ = core::EditMode::ordinary;
    // Ctrl+V を矩形の鍵に写すかどうかを決めるのに要る（ADR 0035 の決定 9）。
    core::VimMode vim_mode_ = core::VimMode::normal;
    // Vim の NORMAL に入る前の IME の開閉。控えが在ることが「いま切ってある」でもある（決定 5）。
    ImeOpenState ime_open_ = ImeOpenState::unrecorded;
    // 前の delivery の入力行の種類。closed_once を入力行の種類が変わったときの 1 回にするために
    // 覚える（無し → Ex・Ex → 面など・ADR 0061 の決定 2・#635 の差し戻し 1）。
    std::optional<core::InputLinePrompt> input_line_;
    // 前の frame で入力行が変換中だったか。入力行が閉じて変換が消えた瞬間を知る（決定 3）。
    bool command_composing_ = false;
    // いま入れ子になっている send の数と、send の途中で届いた合図（何回届いても 1 つ）。
    std::size_t sending_depth_ = 0;
    bool work_waiting_ = false;
    std::unique_ptr<Direct2DRenderer> renderer_;
    HWND window_ = nullptr;
    // 仮の renderer を結ぶ子窓。差し替えまでしか無い（ADR 0132 の決定 2）。差し替えの前に窓を
    // 閉じると、子窓の HWND は本窓の DestroyWindow で先に壊れ、仮の renderer はその後に
    // デストラクタが解放する（決定 7）。abandon とデストラクタは renderer を先に解放する。
    std::unique_ptr<CoverWindow> cover_;
    // 成功した全クライアントの無効化だけを覚え、次の WM_PAINT まで重複要求をまとめる。
    bool paint_pending_ = false;
    ATOM class_ = 0;
    UINT dpi_ = 96;
    bool rendering_failed_ = false;
    // WM_QUERYENDSESSION で ShutdownBlockReasonCreate を立てた印（ADR 0133 の決定 4）。
    bool shutdown_blocked_ = false;
};
} // namespace nenenib::ui::win32
