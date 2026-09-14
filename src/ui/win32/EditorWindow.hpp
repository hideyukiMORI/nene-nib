#pragma once

#include "Direct2DRenderer.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <expected>
#include <memory>

namespace nenenib::ui::win32
{
// 枠なしの編集窓。操作は意図として controller へ渡し、描画は EditorFrame を写すだけ（ARC-011 /
// CPP-017）。
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
    void place_at_screen_centre();
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM word, LPARAM data) noexcept;
    LRESULT dispatch(UINT message, WPARAM word, LPARAM data) noexcept;
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
    bool rendering_failed_ = false;
};
} // namespace nenenib::ui::win32
