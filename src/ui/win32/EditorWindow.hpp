#pragma once

#include "Appearance.hpp"
#include "AutosaveId.hpp"
#include "BodyLayout.hpp"
#include "ClosingRoute.hpp"
#include "ClosingStep.hpp"
#include "CommandEdit.hpp"
#include "Composition.hpp"
#include "CoverWindow.hpp"
#include "DeliveryStep.hpp"
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
#include "LayoutRect.hpp"
#include "LinkCardTarget.hpp"
#include "Milestone.hpp"
#include "MinimapGrab.hpp"
#include "OperationTexts.hpp"
#include "PaneLayouts.hpp"
#include "PaneResize.hpp"
#include "PaneSide.hpp"
#include "PaneSplit.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "PresentedMilestones.hpp"
#include "PreviewPoint.hpp"
#include "PreviewTaskPress.hpp"
#include "RenderFailure.hpp"
#include "RendererSources.hpp"
#include "RgbColor.hpp"
#include "SessionEnd.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPress.hpp"
#include "SettingsTarget.hpp"
#include "SplitGrab.hpp"
#include "SplitLayout.hpp"
#include "SplitShare.hpp"
#include "StartPress.hpp"
#include "StartScreenView.hpp"
#include "StatusBarHit.hpp"
#include "TabCommand.hpp"
#include "TabSweep.hpp"
#include "TimingPort.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "VimMode.hpp"
#include "WheelTurn.hpp"
#include "WindowFailure.hpp"
#include "WindowPorts.hpp"
#include "WindowTimer.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
    create(HINSTANCE instance, application::EditorController &controller, WindowPorts ports,
           RendererSources sources);
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
    EditorWindow(HINSTANCE instance, application::EditorController &controller, WindowPorts ports,
                 RendererSources sources);
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
    // 帯の上の押下を扱ったら true（本文とステータスバーへは流さない）。clicks はタブの上の左の
    // 続けての押し（2 回目以降は SwitchTab の代わりに PinTab の 1 意図・ADR 0179 の決定 4-4）。
    [[nodiscard]] bool click_title_bar(LPARAM data, application::PointerClicks clicks);
    void release_title_bar(LPARAM data);
    void press_middle(LPARAM data);
    void middle_click(LPARAM data);
    void point_at(LPARAM data);
    void hover(const std::optional<core::TitleBarTarget> &target);
    // タブを閉じる流れ（未保存なら切り替えて確かめる・ADR 0056 の決定 6）。
    void close_tab(std::size_t tab);
    void finish_tab_action(std::optional<std::size_t> close_request, bool closing);
    // タブのメニューの「ほかのタブを閉じる」「右のタブを閉じる」（ADR 0078 の追補 #1790）。帯の順に
    // 1 つずつ × と同じ閉じる道で閉じ、未保存のタブでは「保存しますか」（ask_save）を聞いて取消で
    // 止める。アクティブでない相手を先に名指しで閉じ（まだ読んでいないタブは読まない）、アクティブが
    // 相手なら最後に残すタブへ切り替えてから閉じる（切り替えが成り立たなければ止める）。
    void finish_tab_sweep(std::optional<application::TabSweep> sweep);
    void close_tabs_beside(application::TabSweep sweep);
    [[nodiscard]] bool sweep_step(const application::TabSweep &sweep);
    // 帯の位置 tab の未保存のタブを見せて確かめ、home へ戻る。閉じてよければ真。
    [[nodiscard]] bool confirm_swept_tab(std::size_t tab, application::AutosaveId home);
    [[nodiscard]] bool close_swept_tab(std::size_t tab, application::AutosaveId home);
    void finish_operation(std::optional<core::EditorOperation> operation_request);
    // 開いた文書に格子表示を聞く（ADR 0140 の決定 2）。deliver の終わりと、起動の終わり
    // （最初の frame の後）から呼ぶ。
    void ask_display();
    void finish_display_question(bool question);
    [[nodiscard]] LRESULT press_caption(UINT message, WPARAM word, LPARAM data) noexcept;
    void activate_caption(WPARAM word) noexcept;
    // 窓の操作の押しを捕まえている間の WM_LBUTTONUP（#1464）。押しを覚えていなければ false。
    [[nodiscard]] bool release_caption(LPARAM data) noexcept;
    void finish_caption(std::optional<WPARAM> button) noexcept;
    // 左の押し（WM_LBUTTONDOWN と WM_LBUTTONDBLCLK）。起動画面の行で始まった押しの列の 2 回目からは
    // 何もしない（ADR 0172 の追補 24）。ほかは click_client へ。
    void press_left(LPARAM data, application::PointerClicks clicks);
    void click_client(LPARAM data, application::PointerClicks clicks);
    void click_palette(LPARAM data);
    void place_caret(LPARAM data, application::PointerClicks clicks);
    void click_body(LPARAM data, application::PointerClicks clicks);
    void point_text(POINT point, core::SelectionAnchoring anchoring,
                    application::PointerClicks clicks);
    // 点の本文の位置の意図（point_text と右クリックのメニューの共通の 1 本）。
    [[nodiscard]] application::PointText text_point(POINT point, core::SelectionAnchoring anchoring,
                                                    application::PointerClicks clicks) const;

    // ----------------------------------------------------------------
    // 右クリックのメニュー（#1051） EditorWindowContextMenu.cpp の群（ADR 0078
    // の追補）。開く・選ぶ・閉じるは意図を送るだけで、 何を出すか・何で閉じるかは controller
    // が決める。
    // 右の押しと離し。押した点と離した点がどちらも操作中の枠の本文の中なら、離したときに開く。
    void press_right(LPARAM data);
    void release_right(LPARAM data);
    // 帯のタブの上の右の離し（#1790）。押したタブと離したタブが同じときだけ、そのタブのメニューを
    // 帯のすぐ下に開く。開いたら真。
    [[nodiscard]] bool release_right_on_tab(LPARAM data);
    // 点の下の帯のタブ（× の上もタブ）。Ctrl+P の面が開いている間とタブの外は値なし（設定の面と
    // 補完の面の押しは press_right が先に取り、これを読まない）。
    [[nodiscard]] std::optional<core::TitleBarTarget> tab_under(LPARAM data) const;
    [[nodiscard]] bool press_right_on_menu(POINT point);
    // WM_CONTEXTMENU。鍵（Shift+F10・メニューキー）の分だけ（lParam が -1）キャレットの位置に開く。
    [[nodiscard]] bool open_context_menu_from_keys(LPARAM data);
    // メニューが開いている間の左の押しと離しとマウスの動き。開いていなければ false（今の道へ）。
    [[nodiscard]] bool press_context_menu(LPARAM data);
    [[nodiscard]] bool release_context_menu(LPARAM data);
    [[nodiscard]] bool point_context_menu(LPARAM data);
    // 右の押しと離しと WM_CONTEXTMENU の入口。
    [[nodiscard]] LRESULT context_menu_message(UINT message, WPARAM word, LPARAM data);
    // 帯の空きや窓の操作（クライアントの外）を押したら開いているメニューを閉じる。
    void close_context_menu_outside();
    // 点の下のもう 1 つの枠の本文か（プレビューの枠は除く・右クリックはその枠へ移ってから開く）。
    [[nodiscard]] bool in_other_text_body(POINT point) const;
    // メニューが開いている間の鍵（上下・Enter・Esc。ほかの鍵と文字は捨てる）と、焦点を失ったときに
    // 閉じる。受けて捨てたら true（焦点を失ったときは閉じて false・今の道も通す）。
    [[nodiscard]] bool take_context_menu_key(UINT message, WPARAM word);
    // 開いている面（メニュー・設定の面・補完の面）が先に受ける鍵と文字。
    [[nodiscard]] bool take_face_key(UINT message, WPARAM word, LPARAM data);
    void press_context_menu_key(WPARAM word);
    // 補完の面（ADR 0190 の決定 5・EditorWindowCompletion.cpp）。面が見えている間の上下・PageUp /
    // PageDown・Enter・Tab・Esc を面が受けたら真。窓が非アクティブになったら閉じる。
    [[nodiscard]] bool take_completion_key(UINT message, WPARAM word);
    // Enter と Tab。確定が本文を変えなければ面を閉じて偽（鍵は本文へ通す）。
    [[nodiscard]] bool commit_completion();
    void close_completion_outside();
    // 窓が焦点を失ったときの片付け（選択のドラッグ・Ctrl+Tab の歩き・帯の hover・補完の面・hover の
    // 札・字の大きさのホイールの残り）。
    void lose_focus();
    // 面の行のクリック（押した行と離した行が同じときだけ確定）と面の上のホイール（#1844 の R1）。
    // 面が受けたら真で、本文へ渡さない。
    [[nodiscard]] bool press_completion(LPARAM data);
    [[nodiscard]] bool release_completion(LPARAM data);
    void lose_completion_press(UINT message);
    [[nodiscard]] bool close_completion_under(LPARAM data);
    [[nodiscard]] bool scroll_completion(std::int32_t delta, LPARAM data);
    [[nodiscard]] bool in_text_body(POINT point) const;

    // ----------------------------------------------------------------
    // 設定の面（施主決定 D102・#1836） EditorWindowSettings.cpp の群。開いている間の鍵・文字・
    // マウス・ホイール・IME を面の意図にする。開いていなければどれも false / 値なし（今の道へ）。
    // 鍵・文字・マウスを意図の列に写すのは SettingsInput の純関数（窓を作らずに試す）。
    [[nodiscard]] bool take_settings_key(UINT message, WPARAM word, LPARAM data);
    // 面の外の Ctrl+, の長押しの反復か（捨てる＝開閉を繰り返さない）。
    [[nodiscard]] bool swallow_repeated_settings_key(WPARAM word, LPARAM data) const;
    // 面の意図を送ってから、焦点の行が見える所へ行の領域の送りを寄せる。
    void send_settings(const application::EditorIntent &intent);
    void send_settings_all(const std::vector<application::EditorIntent> &intents);
    [[nodiscard]] const std::optional<core::SettingsLayout> &settings_layout_drawn() const;
    // 面が閉じた所で IME の変換を 1 回取り消す（段 cancel_settings_composition・レビュー M1）。
    void follow_settings_closing();
    // 面の外を押して閉じた直後の 2 回目の押しか（読んだら消す）。
    [[nodiscard]] bool take_settings_dismissal() noexcept;
    void press_left_button(UINT message, LPARAM data);
    [[nodiscard]] bool press_settings(LPARAM data);
    [[nodiscard]] bool release_settings(LPARAM data);
    [[nodiscard]] bool press_settings_right(LPARAM data);
    // 点（クライアントの座標）が開いている面の上か。配置を読むだけで frame を作らない。
    [[nodiscard]] bool over_settings(LPARAM data) const;
    [[nodiscard]] bool wheel_settings(std::int32_t delta, LPARAM data);
    [[nodiscard]] core::SettingsTarget settings_target_under(std::int32_t x, std::int32_t y) const;
    [[nodiscard]] std::optional<LRESULT> compose_settings(UINT message, WPARAM word, LPARAM data);
    void place_settings_composition(const core::LayoutRect &search);
    // 左の押しを続けた回数に数える（#1250）。OS のダブルクリックは 2 回目、その後に OS の
    // ダブルクリックの時間と升の中で押せば 3 回目（4 回目以降も 3
    // 回目）。時刻は押しの窓メッセージの 時刻（GetMessageTime）だけを読む。
    [[nodiscard]] application::PointerClicks counted_clicks(application::PointerClicks pressed,
                                                            LPARAM data);
    void begin_selection_drag();
    void end_selection_drag();
    void drag_selection(LPARAM data);
    void scroll_selection();
    // プレビューの枠のチェックボックス（ADR 0155 の追補 24）。押した要素を覚え、離した要素が同じ
    // ときだけ TogglePreviewTask を送る（ADR 0056 の決定 9 の形）。
    void press_preview_task(LPARAM data);
    // プレビューの枠の押し（チェックボックスが無ければリンクの文字を覚える・全面は選択も始める・
    // #1869 の段 L2）。
    void press_side_preview(LPARAM data);
    // プレビューの表の横の送り（#1883・EditorWindowTableScroll.cpp）。棒の上の押しはつかむか 1 枠
    // 送り（押したら真）、つかんだ間の動きと離しと捕まえの喪失はその表へ、Shift+ホイールと横の
    // ホイールは点の下の広い表へ（送ったら真）、動きは載せた棒を知らせる。送りの量と端は
    // application が決める。
    [[nodiscard]] bool press_table_bar(LPARAM data);
    [[nodiscard]] bool drag_table_bar(LPARAM data);
    [[nodiscard]] bool release_table_bar();
    [[nodiscard]] bool wheel_table(std::int32_t delta, LPARAM data);
    void hover_table_bar(LPARAM data);
    void hover_table_bar_of(std::optional<std::size_t> table);
    // クライアントの点がプレビューの上に重なる面（設定の引き出し・Ctrl+P
    // の面・補完の面・右クリックの メニュー・hover の札・リンクの札）の上か（#1884 のレビュー
    // R1・L3）。面の下の表はホイールと 載せたマウスを受けない。判定はそれぞれの面の当たりの 1
    // 本（縦のホイールと押しと同じ）を読む。
    [[nodiscard]] bool faces_cover(std::int32_t x, std::int32_t y) const;
    void press_full_preview(LPARAM data);
    void release_preview_task(LPARAM data);
    // 全面プレビューの描いた文字の上の押しとドラッグ（ADR 0155 の改訂 18）。押した点の文字の位置を
    // renderer の hit test で引いて PointPreview を送り（Shift つきは選択を伸ばす）、捕まえて
    // 離すまで同じ意図で動く端を送る。
    void press_preview_text(LPARAM data);
    void end_preview_drag();
    // プレビューのリンクの札（施主決定 D106・#1869 の段 L2・EditorWindowLinkCard.cpp）。札が出て
    // いる間の Enter・Esc・Ctrl+C を受けたら真。札の上の押しと離しはその部品だけ（受けたら真）で、
    // 札の外の押しは札を閉じて偽（今までの道へ）。プレビューの文字の押しを覚え、離した文字と組んで
    // ClickPreviewLink を送る。
    [[nodiscard]] bool take_link_card_key(UINT message, WPARAM word, LPARAM data);
    [[nodiscard]] bool follow_link_card_enter(UINT message, WPARAM word, LPARAM data);
    [[nodiscard]] bool press_link_card_key(WPARAM word);
    [[nodiscard]] bool press_link_card(LPARAM data);
    [[nodiscard]] bool release_link_card(LPARAM data);
    void press_preview_link(LPARAM data);
    void release_preview_link(LPARAM data);
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
    // 意図を送った後の 1 段を写す（順は delivery_order・要るかは delivery_step_due・#1854）。
    // 枠の配置・境界の比・操作中の側（変わったら寸法を送り直す・ADR 0148 の 2026-11-22 の追補）。
    using PaneArrangement = std::pair<core::PaneSplit, core::PaneSide>;
    [[nodiscard]] PaneArrangement pane_arrangement() const noexcept;
    void delivery_step(DeliveryStep step, PaneArrangement previous_layout,
                       application::EditorDelivery &delivery);
    // 合図のメッセージを受けた。send の途中なら覚えるだけ、そうでなければ送る。
    void receive_work();
    // WM_TIMER を窓のタイマーへ写す。自動退避の合図は AutosaveTick を 1 回送る（ADR 0137
    // の決定 1）。
    void receive_timer(WPARAM word);
    void start_signal_timers();
    void follow_preview_timer();
    void stop_preview_timer();
    [[nodiscard]] bool previewing() const;
    [[nodiscard]] bool full_previewing() const;
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
    // Vim の INSERT の補完の鍵（#1845）。扱ったら true。
    [[nodiscard]] bool press_vim_completion_key(WPARAM word);
    void run_tab_command(core::TabCommand command);
    void press_command_key(WPARAM word);
    void press_command_control_key(WPARAM word);
    [[nodiscard]] bool press_palette_key(WPARAM word);
    [[nodiscard]] bool press_find_key(WPARAM word);
    void send_palette_or(core::CommandEdit edit,
                         const std::optional<application::EditorIntent> &otherwise);
    void press_plain_key(WPARAM word);
    void press_vim_key(WPARAM word);
    void press_control_key(WPARAM word);
    void send_word_deletion(WPARAM word);
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
    // 窓を壊す唯一の口（ADR 0059 の決定 3・ADR 0133 の決定 1）。先に窓を隠し、前回のタブの一覧を
    // 書いてから壊す（closing_steps(window_closes)・追補 #1340）。end_window と、最後の 1 つを
    // 閉じる close_tab が通る。デストラクタは作りかけの窓の後始末だけ。
    void leave(application::SessionEnd reason);
    // 閉じる手順の列を 1 段ずつ写す（ADR 0133 の追補 #1340）。destroy_window の段を持つ道は
    // window_closes だけで、それを渡すのは leave だけ。WM_ENDSESSION は session_ends を渡す。
    void close_through(ClosingRoute route, const application::EndSession &intent);
    // 閉じる手順の 1 段を写す。close_through だけが呼ぶ。
    void close_step(ClosingStep step, const application::EndSession &intent);
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
    // 聞き直す。箱を出せなかったときは unsaved_kept_ を立てて閉じる側へ。いつも true。
    [[nodiscard]] bool confirm_lost_tab();
    void update_title(const application::EditorDelivery &frame);
    void announce(const application::EditorDelivery &frame);
    void announce_settings(const application::EditorDelivery &frame);
    void offer_utf8(const core::FilePath &path);
    void turn_wheel(WPARAM word, LPARAM data);
    [[nodiscard]] bool over_title_bar(LPARAM data) const;
    [[nodiscard]] bool over_other_pane(LPARAM data) const;
    void follow_split(PaneArrangement previous, application::EditorDelivery &delivery);
    void follow_explorer(application::EditorDelivery &delivery);
    void follow_minimap(application::EditorDelivery &delivery);
    void choose_explorer_root();
    // エクスプローラーの面の群（EditorWindowExplorer.cpp・ADR 0162 の決定 4）。
    void scroll_explorer(std::int32_t delta);
    [[nodiscard]] bool over_explorer(LPARAM data) const;
    void press_explorer_key(WPARAM word);
    [[nodiscard]] std::optional<std::size_t> explorer_row_at(LPARAM data) const;
    [[nodiscard]] bool point_explorer(UINT message, LPARAM data);
    // マウスを載せた面の行（#1390）。点の下の画面の行を送り、窓を出たら値なしを送る。
    void hover_explorer_row(LPARAM data);
    void hover_explorer_line(std::optional<std::size_t> line);
    // 起動画面の行のクリック（ADR 0172 の決定 4・追補 24・EditorWindowPointer.cpp）。点の下の選べる
    // 行は描画と同じ core::start_screen_layout の 1 本から引く。押しは行の上なら行の位置と中身を
    // 覚えて真（本文のキャレットを動かさない）、離しは同じ位置の今の行の中身が押した行と同じときだけ
    // ChooseStartEntry を送る（ADR 0056 の決定 9・core::start_released）。マウスの動くたびには
    // 呼ばない。
    [[nodiscard]] std::optional<std::size_t> start_row_at(const application::StartScreenView &view,
                                                          LPARAM data) const;
    [[nodiscard]] bool press_start_entry(LPARAM data);
    void release_start_entry(LPARAM data);
    // 最初の frame を画面に出した直後に FirstFramePresented を 1 回だけ送る（ADR 0172 の決定 3）。
    void signal_first_frame();
    // 面と本文の境のドラッグ（ADR 0162 の追補 D78）。押す・動かす・離す・捕まえを失う・ダブル
    // クリック・カーソル。
    [[nodiscard]] bool explorer_divider_at(std::int32_t x, std::int32_t y) const;
    [[nodiscard]] bool press_explorer_divider(LPARAM data);
    void drag_explorer_divider(LPARAM data);
    [[nodiscard]] bool release_explorer_divider(LPARAM data);
    void lose_explorer_divider();
    [[nodiscard]] bool cancel_explorer_divider();
    [[nodiscard]] bool take_explorer_escape(LPARAM data);
    // 面の幅かミニマップのドラッグ中の Esc を受けたか（key_message の入口）。
    [[nodiscard]] bool take_drag_escape(WPARAM word, LPARAM data);
    void settle_explorer_divider(LPARAM point);
    [[nodiscard]] bool explorer_divider_under(LPARAM point) const;
    [[nodiscard]] bool reset_explorer_width(LPARAM data);
    void hover_explorer_divider(LPARAM data);
    void end_hidden_explorer_drag();
    void release_left(LPARAM data);
    void move_pointer(LPARAM data);
    // ミニマップの面の押し・ドラッグ・離し（ADR 0171 の決定 5 の追補・EditorWindowPointer.cpp）。
    // 覚えの次の値と送る意図は minimap_step の 1 本。面は描画と同じ core::minimap_layout から引く。
    [[nodiscard]] std::optional<core::LayoutRect> minimap_panel_under(LPARAM data) const;
    [[nodiscard]] bool press_minimap(LPARAM data);
    [[nodiscard]] bool drag_minimap(LPARAM data);
    [[nodiscard]] bool release_minimap(LPARAM data);
    void end_minimap_drag(UINT message);
    [[nodiscard]] bool take_minimap_escape(LPARAM data);
    [[nodiscard]] bool explorer_divider_cursor(LPARAM data) const;
    // 2 枠の境界のドラッグ（EditorWindowSplit.cpp・ADR 0148 の 2026-11-22 の追補の決定 3）。
    [[nodiscard]] std::optional<std::int32_t> split_divider_start(LPARAM point) const;
    [[nodiscard]] std::optional<core::SplitShare> step_split(UINT message, LPARAM data);
    [[nodiscard]] bool press_split_divider(LPARAM data);
    void drag_split_divider(LPARAM data);
    [[nodiscard]] bool release_split_divider(LPARAM data);
    void lose_split_divider();
    [[nodiscard]] bool cancel_split_divider();
    [[nodiscard]] bool take_split_escape(LPARAM data);
    void settle_split_divider(LPARAM point);
    [[nodiscard]] bool reset_split_share(LPARAM data);
    void hover_split_divider(LPARAM data);
    [[nodiscard]] bool split_divider_cursor(LPARAM data) const;
    [[nodiscard]] bool divider_cursor(LPARAM data) const;
    void lose_dividers();
    void forget_divider_hover() noexcept;
    [[nodiscard]] std::int32_t explorer_width_at(std::int32_t x) const;
    // 操作 → メンバ関数の表を引く（EditorWindowOperations.cpp・ADR 0078 の追補 #1188）。操作ごとの
    // 関数は引数なしで、本体は前の run_operation の case のまま。
    using OperationFunction = void (EditorWindow::*)();
    [[nodiscard]] static OperationFunction runner_of(core::EditorOperation operation) noexcept;
    // 表の本体（定義は EditorWindowOperations.cpp）。操作が 55 になり、表を runner_of の中に置くと
    // 関数の長さの上限を超えたので、private の関数を指せる static
    // メンバの変数にした（#1790）。宣言は static const、定義は constexpr で、runner_of の
    // static_assert は定義の後ろにあるので表を読める。
    static const std::array<std::pair<core::EditorOperation, OperationFunction>,
                            core::operation_texts.size()>
        operation_runners;
    void run_new_tab();
    void run_close_tab();
    void run_recent_tab();
    void run_recent_tab_back();
    void run_list_files();
    void run_list_operations();
    void run_toggle_bookmark();
    void run_find();
    void run_find_next();
    void run_find_previous();
    void run_replace();
    void run_go_to_line();
    void run_undo();
    void run_redo();
    void run_paste_plain();
    void run_paste_html();
    void run_font_larger();
    void run_font_smaller();
    void run_font_reset();
    void run_split_vertical();
    void run_split_horizontal();
    void run_next_pane();
    void run_close_pane();
    void run_toggle_preview();
    void run_preview_full();
    void run_toggle_explorer();
    void run_insert_date_time();
    void run_choose_encoding();
    void run_choose_language();
    void run_show_version();
    void run_search_project();
    void run_cut();
    void run_copy();
    void run_paste();
    void run_delete_selection();
    void run_select_all();
    void run_cycle_finder_preview();
    void run_complete();
    void run_go_to_definition();
    void run_show_information();
    void run_open_settings();
    // タブの 5 つ（ADR 0078 の追補 #1790）。F1 の一覧からはアクティブなタブに効く。
    void run_reveal_in_explorer();
    void run_copy_path();
    void run_copy_file_name();
    void run_close_other_tabs();
    void run_close_right_tabs();
    // 診断の 4 つ（#1939）。
    void run_next_diagnostic();
    void run_previous_diagnostic();
    void run_show_diagnostic();
    void run_list_diagnostics();
    void run_widen_pane();
    void run_narrow_pane();
    void run_even_panes();
    void resize_operated_pane(core::PaneResize resize);
    void run_open_language_server_log();
    // 参照の一覧（#1947）。
    void run_list_references();
    // 記号の一覧（#1955）。
    void run_list_symbols();
    // 引数の情報を表示（#1959）。
    void run_show_signature_help();
    // 文書を整形・選択範囲を整形（#1993）。
    void run_format_document();
    void run_format_selection();
    // 名前を変更（#2004）。
    void run_rename_symbol();
    void run_choose_line_ending();
    void run_print();
    void run_register_shell();
    void run_unregister_shell();
    [[nodiscard]] bool keeps_block_key(core::EditorOperation operation) const noexcept;
    void scroll_tabs(std::int32_t delta);
    void zoom_wheel(std::int32_t delta);
    void scroll_palette(std::int32_t delta);
    void scroll_body(std::int32_t delta, LPARAM data);
    void read_wheel_setting();
    // タブの札の待ち（#1789）に OS の載せの待ち時間を読む。起動と WM_SETTINGCHANGE の 2 か所。
    void read_hover_setting();
    // Windows の「アニメーション効果」（SPI_GETCLIENTAREAANIMATION）を読む（ADR 0192 の決定
    // 6・起動と WM_SETTINGCHANGE）。読めなければ動かす側（既定のオン）。
    void read_animation_setting();
    // 描いた後にキャレットの動きと打ったときの粒を追う（ADR 0192 の決定 3・追補・#1979）。どちらか
    // が生きている間だけ合図のタイマーを張り、番号が変わったら経過の起点を測り直す。両方が止まった
    // らタイマーを外す。
    void follow_caret_motion();
    // 合図のタイマーを張っていれば外す。
    void stop_caret_motion();
    // 合図のタイマーが切れたら、今の動きの番号と経過を CaretMotionTick で送る。窓が最小化されて
    // いるか見えなければ送らずにタイマーを外し、renderer の覚えを捨てる（#1956 の R1）。
    void send_caret_motion();
    [[nodiscard]] application::VisibleLines body_view() const;
    [[nodiscard]] core::PaneLayouts body_panes(core::FontSize font_size) const;
    // 本文の枠を置く client（面とミニマップを除いた所・body_panes と境界のドラッグが共用）。
    [[nodiscard]] core::LayoutRect pane_client() const;
    [[nodiscard]] core::ExplorerLayout explorer_frame() const;
    [[nodiscard]] core::BodyLayout active_body(const core::EditorSettings &settings) const;

    HINSTANCE instance_;
    application::EditorController &controller_;
    application::TimingPort &timing_;
    // フォルダ選びの箱の WSL の場所（ADR 0162 の追補 #1450）。箱を出すときだけ読む。
    application::WslDistributionPort &wsl_;
    // キャレットの動きの経過を測る時計（ADR 0192 の決定 3）。動いている間だけ読む。
    application::MotionClockPort &clock_;
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
    // 表の横の送りのホイールの刻みに満たない分（DIP に変える前の WHEEL_DELTA の単位・#1883）と、
    // 棒をつかんだ表の通し番号（捕まえを持つ間）。
    std::int32_t table_wheel_remainder_ = 0;
    std::optional<std::size_t> table_dragging_;
    // 本文のホイールの余り（「delta × 1 刻みの行数」の単位・WheelTurn）と、OS の
    // SPI_GETWHEELSCROLLLINES の値（起動と WM_SETTINGCHANGE で読む・#499）。
    std::int32_t body_wheel_remainder_ = 0;
    // 面の一覧のホイールの余り（単位は本文と同じ・#737）。本文の余りとは別の対象なので共有しない。
    std::int32_t palette_wheel_remainder_ = 0;
    // エクスプローラーの面のホイールの余り（ADR 0162 の決定 4）。
    std::int32_t explorer_wheel_remainder_ = 0;
    // 面の上で左を押して、まだ離していない（離したときにだけ PointExplorer の released を送る）。
    bool explorer_pressed_ = false;
    // 境を押してから離すまで（D78）。値は押した点と面の右端のずれ（物理画素）で、動かした点から
    // ずれを引いた所を面の右端にする。grab_x は押した点の x（同じ点の WM_MOUSEMOVE を幅の変更に
    // しない）、dragged は最後に送った幅（DIP・捕まえを失ったときに離したのと同じに送る）。
    std::optional<std::int32_t> explorer_grab_offset_;
    std::int32_t explorer_grab_x_ = 0;
    std::optional<std::int32_t> explorer_dragged_;
    // 捕まえている間の最後の点（押した点か WM_MOUSEMOVE の点）。Esc で取り消した時点のカーソルを
    // この点で決め直す（#1413・OS のカーソルの位置は読まない）。
    LPARAM explorer_drag_point_ = 0;
    // Esc で取り消した後、その Esc の繰り返し（lParam の bit 30）を食べている間（#1413 の L-1）。
    bool explorer_escape_held_ = false;
    // ミニマップの面を押してから離すまで（ADR 0171 の決定 5 の追補）。値があるのは窓が捕まえて
    // いる間だけ。Esc で終えた後、その Esc の繰り返しを食べている間（explorer_escape_held_
    // と同じ）。
    std::optional<MinimapGrab> minimap_grab_;
    bool minimap_escape_held_ = false;
    // 最後の WM_MOUSEMOVE の点が境の上だったか（カーソルを左右の矢印にする・D78）。
    bool over_explorer_divider_ = false;
    // 2 枠の境界を押してから離すまで（ADR 0148 の 2026-11-22 の追補の決定 3）。値があるのは窓が
    // 捕まえている間だけ。次の値は split_step の 1 本（SplitPress.hpp）。
    std::optional<SplitGrab> split_grab_;
    // 捕まえている間の最後の点（Esc で取り消した時点のカーソルをこの点で決め直す・面の境と同じ）。
    LPARAM split_drag_point_ = 0;
    bool split_escape_held_ = false;
    // 最後の WM_MOUSEMOVE の点が境界の上だったか（カーソルを矢印にする）。
    bool over_split_divider_ = false;
    UINT wheel_setting_ = default_wheel_scroll_lines;
    UINT hover_setting_ = default_tab_tip_milliseconds;
    // Windows の「アニメーション効果」がオンか（ADR 0192 の決定 6）。オフならキャレットは跳ぶ。
    bool animation_setting_ = true;
    // 今の動きの番号と、その動きが始まった時計の値（ADR 0192 の決定 3）。合図のタイマーを張って
    // いるか。番号は描く前に renderer へ下限として渡す（作り直した renderer が controller に残った
    // 古い進みと同じ番号を使わない・#1956 の R2）。
    std::uint32_t caret_motion_generation_ = 0;
    std::uint64_t caret_motion_start_ = 0;
    bool caret_motion_timing_ = false;
    // 打ったときの粒の時計の番号と、その時計が始まった時計の値（ADR 0192 の追補・#1979）。合図のタ
    // イマーは動きと共用（caret_motion_timing_）。番号は描く前に renderer へ下限として渡す。
    std::uint32_t caret_sparks_generation_ = 0;
    std::uint64_t caret_sparks_start_ = 0;
    // 帯で押した要素（左ボタンと中ボタンで 1 つずつ）。離した要素と同じときだけ動かし、離したら
    // 消す（ADR 0056 の決定 9）。
    std::optional<core::TitleBarTarget> left_pressed_;
    std::optional<core::TitleBarTarget> middle_pressed_;
    // 起動画面で押した行の位置と中身（離した行と同じときだけ実行し、離したら消す・窓の外へ出た・
    // 捕まえが変わった・起動画面が消えたときも消す・ADR 0172 の決定 4・追補 24）。
    std::optional<core::StartPress> start_pressed_;
    // 起動画面の行で始まった押しの列か（2 回目・3 回目の押しを本文へ渡さない・追補 24）。
    bool start_click_began_ = false;
    // 右の押しの点（本文の中で押したときだけ・離したら消す）と、開いたメニューの上で左を押したか
    // （#1051）。
    std::optional<POINT> right_pressed_;
    // 右の押しを受けた帯のタブ（#1790・離したタブと比べる）。
    std::optional<core::TitleBarTarget> tab_right_pressed_;
    bool context_menu_pressed_ = false;
    // 補完の面の上で押した行（離すまで・面の余白の押しは値なし）と、面のホイールの余り。
    std::optional<std::size_t> completion_pressed_;
    // 補完の面の上で始まった押しの列か（2 回目・3 回目の押しを本文へ渡さない・#1844 の R1b）。
    bool completion_click_began_ = false;
    std::int32_t completion_wheel_remainder_ = 0;
    // 設定の面の上で押した部品と行の身元（離したときに同じなら動かす・#1945 の
    // R2）・ホイールの余り・探す欄へ打つ 文字の上位サロゲート（#1836）。
    std::optional<SettingsPress> settings_pressed_;
    std::int32_t settings_wheel_remainder_ = 0;
    wchar_t settings_high_surrogate_ = 0;
    // 直前の delivery で面が開いていたか（閉じた所を見つける）と、面の外の押しで閉じた直後か。
    bool settings_shown_ = false;
    bool settings_dismissed_ = false;
    // FirstFramePresented を送ったか（窓で 1 回だけ・ADR 0172 の決定 3）。
    bool first_frame_signaled_ = false;
    // プレビューの枠で押したチェックボックス（離したとき・次に押したときに消す）。
    PreviewTaskPress preview_task_press_;
    // プレビューの文字の上で押した位置と、リンクの札の上で押した部品（離したとき・次に押したときに
    // 消す・#1869 の段 L2）。
    std::optional<core::PreviewPoint> preview_link_pressed_;
    std::optional<LinkCardTarget> link_card_pressed_;
    // 札を Enter で開いてから、その Enter がまだ離されていない（反復を捨てる・#1876 のレビュー）。
    bool link_card_enter_held_ = false;
    // OSの捕捉の寿命だけ。選択のanchor/caretの正本はcontrollerにある。
    bool selecting_ = false;
    // 全面プレビューの文字をドラッグで選んでいる間（ADR 0155 の改訂 18）。
    bool preview_dragging_ = false;
    POINT selection_point_{};
    // 直前の左の押しの回数・時刻・点（counted_clicks だけが読み書きする）。
    application::PointerClicks last_clicks_ = application::PointerClicks::single;
    LONG last_click_time_ = 0;
    POINT last_click_point_{};
    static constexpr UINT_PTR selection_timer = std::to_underlying(WindowTimer::selection);
    static constexpr UINT_PTR autosave_timer = std::to_underlying(WindowTimer::autosave);
    static constexpr UINT_PTR preview_timer = std::to_underlying(WindowTimer::preview);
    static constexpr UINT_PTR language_server_timer =
        std::to_underlying(WindowTimer::language_server);
    // プレビューの合図のタイマーを張っているか。開いているかの正本は application の枠の状態。
    bool preview_ticking_ = false;
    // 最後に本文の寸法を送ったときにエクスプローラーの面が出ていたか（ADR 0162 の決定 2）。
    bool shown_explorer_ = false;
    // 同じくミニマップが出ていたか（ADR 0171 の決定 2・controller の minimap_shown の写し）。
    bool shown_minimap_ = false;
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
    // 描画を失った後の「保存しますか」を出せなかった印。leave は未保存を捨てたことにしない
    // （自動退避を残す・ADR 0133 の追補 #1282）。
    bool unsaved_kept_ = false;
    // WM_QUERYENDSESSION で ShutdownBlockReasonCreate を立てた印（ADR 0133 の決定 4）。
    bool shutdown_blocked_ = false;
};
} // namespace nenenib::ui::win32
