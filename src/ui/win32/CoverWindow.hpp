#pragma once

#include "WindowFailure.hpp"

#include <windows.h>

#include <expected>
#include <memory>

namespace nenenib::ui::win32
{
// 起動の最初の frame を WARP で描くための、本窓の client を覆う子窓（ADR 0132 の決定 2・5・7）。
// 本窓の HWND に WARP の swap chain を結ばないために、仮の renderer はこの窓に結ぶ。
// 入力は受けない（WM_NCHITTEST は HTTRANSPARENT・WS_EX_TRANSPARENT）。本物の device へ
// 差し替えたら壊す。本窓が先に壊れたときは子窓も一緒に壊れているので、何もしない。
class CoverWindow final
{
  public:
    [[nodiscard]] static std::expected<std::unique_ptr<CoverWindow>, WindowFailure>
    create(HINSTANCE instance, HWND parent);
    ~CoverWindow();
    CoverWindow(const CoverWindow &) = delete;
    CoverWindow(CoverWindow &&) = delete;
    CoverWindow &operator=(const CoverWindow &) = delete;
    CoverWindow &operator=(CoverWindow &&) = delete;

    // 本窓の client と同じ大きさに動かす（差し替えの前の WM_SIZE・決定 5）。
    void resize(const RECT &client) noexcept;
    [[nodiscard]] HWND handle() const noexcept;

  private:
    explicit CoverWindow(HINSTANCE instance);
    [[nodiscard]] std::expected<void, WindowFailure> initialize(HWND parent);
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM word, LPARAM data) noexcept;
    static void forget(HWND window) noexcept;

    HINSTANCE instance_;
    HWND window_ = nullptr;
    ATOM class_ = 0;
};
} // namespace nenenib::ui::win32
