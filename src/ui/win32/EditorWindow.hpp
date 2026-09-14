#pragma once

#include "Direct2DRenderer.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "StatusBarHit.hpp"
#include "TitleBarBackdrop.hpp"
#include "TitleBarHit.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <expected>
#include <memory>

namespace nenenib::ui::win32
{
// 枠なしの編集窓。操作は意図として controller へ渡し、描画は EditorFrame を写すだけ（ARC-011 /
// CPP-017）。タイトルバーとステータスバーの位置は core のレイアウト純関数が決める（ADR 0008）。
class EditorWindow final
{
  public:
    [[nodiscard]] static std::expected<std::unique_ptr<EditorWindow>, WindowFailure>
    create(HINSTANCE instance, application::EditorController &controller);
    ~EditorWindow();
    EditorWindow(const EditorWindow &) = delete;
    EditorWindow(EditorWindow &&) = delete;
    EditorWindow &operator=(const EditorWindow &) = delete;
    EditorWindow &operator=(EditorWindow &&) = delete;

    [[nodiscard]] bool rendering_failed() const noexcept;

  private:
    EditorWindow(HINSTANCE instance, application::EditorController &controller);
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
    void present(const application::EditorFrame &frame);
    void abandon();
    void refresh_appearance();
    void resize();
    void change_dpi(WPARAM word, LPARAM data);

    HINSTANCE instance_;
    application::EditorController &controller_;
    std::unique_ptr<Direct2DRenderer> renderer_;
    HWND window_ = nullptr;
    ATOM class_ = 0;
    UINT dpi_ = 96;
    TitleBarBackdrop backdrop_ = TitleBarBackdrop::opaque;
    bool rendering_failed_ = false;
};
} // namespace nenenib::ui::win32
