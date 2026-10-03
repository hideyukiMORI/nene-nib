#pragma once

#include "Composition.hpp"
#include "Direct2DRenderer.hpp"
#include "EditMode.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "FilePath.hpp"
#include "HistoryDirection.hpp"
#include "ImeOpenState.hpp"
#include "ImeStance.hpp"
#include "Milestone.hpp"
#include "RenderFailure.hpp"
#include "SessionEnd.hpp"
#include "StatusBarHit.hpp"
#include "TabCommand.hpp"
#include "TimingPort.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "VimMode.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace nenenib::ui::win32
{
// 枠なしの編集窓。操作は意図として controller へ渡し、描画は EditorFrame を写すだけ（ARC-011 /
// CPP-017）。タイトルバーとステータスバーの位置は core のレイアウト純関数が決める（ADR 0008）。
// 描画は WM_PAINT の 1 か所に集める。意図を適用したら無効化するだけで、まとめて来た入力は
// 1 フレームで描かれる（ADR 0011 の決定 6）。節目は打つだけで、時刻は知らない（決定 1）。
class EditorWindow final
{
  public:
    [[nodiscard]] static std::expected<std::unique_ptr<EditorWindow>, WindowFailure>
    create(HINSTANCE instance, application::EditorController &controller,
           application::TimingPort &timing);
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

  private:
    EditorWindow(HINSTANCE instance, application::EditorController &controller,
                 application::TimingPort &timing);
    [[nodiscard]] std::expected<void, WindowFailure> initialize();
    [[nodiscard]] std::expected<void, WindowFailure> start_rendering();
    void apply_backdrop(const application::EditorFrame &frame);
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
    [[nodiscard]] LRESULT press_caption(UINT message, WPARAM word, LPARAM data) noexcept;
    void activate_caption(WPARAM word) noexcept;
    void click_client(LPARAM data);
    void click_palette(LPARAM data);
    void place_caret(LPARAM data);
    // 描く唯一の口。Present が返った直後に frame_presented を打つ（ADR 0011 の決定 1）。
    [[nodiscard]] std::expected<void, RenderFailure>
    draw_frame(const application::EditorFrame &frame);
    void paint();
    // 意図の適用で変わった見た目は、次の WM_PAINT でまとめて描く（決定 6）。
    void invalidate() noexcept;
    void present(const application::EditorFrame &frame);
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
    void type_character(WPARAM word);
    void type_text(std::string utf8);
    void press_key(WPARAM word);
    [[nodiscard]] bool press_bookmark_key(WPARAM word, LPARAM data);
    // タブの鍵（ADR 0056 の決定 10）。扱ったら true。
    [[nodiscard]] bool press_tab_key(WPARAM word);
    void run_tab_command(core::TabCommand command);
    void press_command_key(WPARAM word);
    void press_command_control_key(WPARAM word);
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
    // frame の構え（ImeStance）を実行する。決めるのは application（ADR 0061 の決定 2）。
    void follow_ime(const application::EditorFrame &frame);
    void close_ime();
    void restore_ime();
    // IME の側に残った変換を取り消す（ImmNotifyIME の CPS_CANCEL・ADR 0061 の決定 3）。
    void cancel_ime_composition();
    // Ctrl+Z / Ctrl+Y は Vim では u / Ctrl-r に譲り、Ctrl+R は Vim のときだけ意味を持つ。
    void send_history(core::HistoryDirection direction);
    void send_vim_redo();
    void open_document();
    void save_document();
    void save_document_as();
    void close_window();
    // 窓を壊す直前の 1 か所（ADR 0059 の決定 3）。前回のタブの一覧を書いてから壊す。
    // close_window と、最後の 1 つを閉じる close_tab が通る。abandon とデストラクタは通らない。
    void leave(application::SessionEnd reason);
    // 前回のタブの一覧を書く意図を送る（leave と WM_ENDSESSION）。この後は窓へ描かない。
    void remember_session(application::SessionEnd reason);
    // 未保存なら聞く。閉じる・開き直すのを続けてよいときだけ true（ADR 0010 の決定 10）。
    [[nodiscard]] bool confirm_discard();
    void update_title(const application::EditorFrame &frame);
    void announce(const application::EditorFrame &frame);
    void announce_settings(const application::EditorFrame &frame);
    void offer_utf8(const core::FilePath &path);
    void turn_wheel(WPARAM word, LPARAM data);
    [[nodiscard]] bool over_title_bar(LPARAM data) const;
    void scroll_tabs(std::int32_t delta);
    void zoom_wheel(std::int32_t delta);
    [[nodiscard]] std::size_t body_lines() const;

    HINSTANCE instance_;
    application::EditorController &controller_;
    application::TimingPort &timing_;
    // 題名は変わったときだけ OS へ渡す。毎フレーム SetWindowTextW を呼ばない（決定 13）。
    std::wstring window_title_;
    // WM_CHAR は UTF-16 の 1 単位ずつ来るので、サロゲートの上位を次の下位まで預かる（ADR 0009）。
    wchar_t pending_high_surrogate_ = 0;
    std::int32_t zoom_wheel_remainder_ = 0;
    std::int32_t tab_wheel_remainder_ = 0;
    // 帯で押した要素（左ボタンと中ボタンで 1 つずつ）。離した要素と同じときだけ動かし、離したら
    // 消す（ADR 0056 の決定 9）。
    std::optional<core::TitleBarTarget> left_pressed_;
    std::optional<core::TitleBarTarget> middle_pressed_;
    // いまの編集モード。鍵をどちらの表で引くかを決めるだけで、正本は EditorState（ARC-004）。
    core::EditMode mode_ = core::EditMode::ordinary;
    // Ctrl+V を矩形の鍵に写すかどうかを決めるのに要る（ADR 0035 の決定 9）。
    core::VimMode vim_mode_ = core::VimMode::normal;
    // Vim の NORMAL に入る前の IME の開閉。控えが在ることが「いま切ってある」でもある（決定 5）。
    ImeOpenState ime_open_ = ImeOpenState::unrecorded;
    // 前に実行した構え。closed_once を入るときの 1 回にするために覚える（ADR 0061 の決定 2）。
    std::optional<application::ImeStance> ime_stance_;
    // 前の frame で面の入力行が変換中だったか。面が閉じて変換が消えた瞬間を知る（決定 3）。
    bool command_composing_ = false;
    // いま入れ子になっている send の数と、send の途中で届いた合図（何回届いても 1 つ）。
    std::size_t sending_depth_ = 0;
    bool work_waiting_ = false;
    std::unique_ptr<Direct2DRenderer> renderer_;
    HWND window_ = nullptr;
    ATOM class_ = 0;
    UINT dpi_ = 96;
    bool rendering_failed_ = false;
};
} // namespace nenenib::ui::win32
