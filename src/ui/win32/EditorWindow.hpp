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
#include "Milestone.hpp"
#include "RenderFailure.hpp"
#include "StatusBarHit.hpp"
#include "TimingPort.hpp"
#include "TitleBarHit.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <cstddef>
#include <expected>
#include <memory>
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
    void send(const application::EditorIntent &intent);
    void type_character(WPARAM word);
    void type_text(std::string utf8);
    void press_key(WPARAM word);
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
    // NORMAL では IME を切り、INSERT と通常モードでは切る前の開閉に戻す（決定 5）。
    void follow_ime(const application::EditorFrame &frame);
    void close_ime();
    void restore_ime();
    // Ctrl+Z / Ctrl+Y は Vim では u / Ctrl-r に譲り、Ctrl+R は Vim のときだけ意味を持つ。
    void send_history(core::HistoryDirection direction);
    void send_vim_redo();
    void open_document();
    void save_document();
    void save_document_as();
    void close_window();
    // 未保存なら聞く。閉じる・開き直すのを続けてよいときだけ true（ADR 0010 の決定 10）。
    [[nodiscard]] bool confirm_discard();
    void update_title(const application::EditorFrame &frame);
    void announce(const application::EditorFrame &frame);
    void announce_settings(const application::EditorFrame &frame);
    void offer_utf8(const core::FilePath &path);
    void turn_wheel(WPARAM word);
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
    // いまの編集モード。鍵をどちらの表で引くかを決めるだけで、正本は EditorState（ARC-004）。
    core::EditMode mode_ = core::EditMode::ordinary;
    // Vim の NORMAL に入る前の IME の開閉。控えが在ることが「いま切ってある」でもある（決定 5）。
    ImeOpenState ime_open_ = ImeOpenState::unrecorded;
    std::unique_ptr<Direct2DRenderer> renderer_;
    HWND window_ = nullptr;
    ATOM class_ = 0;
    UINT dpi_ = 96;
    bool rendering_failed_ = false;
};
} // namespace nenenib::ui::win32
