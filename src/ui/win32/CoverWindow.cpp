#include "CoverWindow.hpp"

#include <bit>

namespace nenenib::ui::win32
{
namespace
{
constexpr wchar_t cover_class_name[] = L"NeNeNib.Cover";
} // namespace

CoverWindow::CoverWindow(HINSTANCE instance) : instance_(instance) {}

CoverWindow::~CoverWindow()
{
    // 本窓が先に壊れていれば、子窓の WM_NCDESTROY が window_ を空にしている。
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }
    if (class_ != 0)
    {
        UnregisterClassW(cover_class_name, instance_);
    }
}

std::expected<std::unique_ptr<CoverWindow>, WindowFailure> CoverWindow::create(HINSTANCE instance,
                                                                               HWND parent)
{
    auto cover = std::unique_ptr<CoverWindow>(new CoverWindow(instance));
    const auto ready = cover->initialize(parent);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    return cover;
}

std::expected<void, WindowFailure> CoverWindow::initialize(HWND parent)
{
    WNDCLASSEXW registration{};
    registration.cbSize = static_cast<UINT>(sizeof(registration));
    registration.hInstance = instance_;
    registration.lpfnWndProc = procedure;
    registration.lpszClassName = cover_class_name;
    class_ = RegisterClassExW(&registration);
    if (class_ == 0)
    {
        return std::unexpected(WindowFailure::class_registration);
    }
    RECT client{};
    GetClientRect(parent, &client);
    // WS_EX_NOREDIRECTIONBITMAP: 本窓と同じく DirectComposition の絵だけを出す。
    // WS_EX_TRANSPARENT と HTTRANSPARENT で、マウスは本窓へ落ちる（決定 5）。
    window_ = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT, cover_class_name, L"",
                              WS_CHILD | WS_VISIBLE, 0, 0, client.right, client.bottom, parent,
                              nullptr, instance_, nullptr);
    if (window_ == nullptr)
    {
        return std::unexpected(WindowFailure::creation);
    }
    SetWindowLongPtrW(window_, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));
    return {};
}

void CoverWindow::resize(const RECT &client) noexcept
{
    if (window_ != nullptr)
    {
        SetWindowPos(window_, nullptr, 0, 0, client.right - client.left, client.bottom - client.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

HWND CoverWindow::handle() const noexcept
{
    return window_;
}

// 本窓と一緒に壊れたときも、ここで所有者に知らせる（デストラクタは何もしない）。
void CoverWindow::forget(HWND window) noexcept
{
    auto *self = std::bit_cast<CoverWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self != nullptr)
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        self->window_ = nullptr;
    }
}

LRESULT CALLBACK CoverWindow::procedure(HWND window, UINT message, WPARAM word,
                                        LPARAM data) noexcept
{
    // OS のメッセージ番号は開いた集合なので、既定分岐だけは許される（CPP-017）。
    switch (message)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_PAINT:
        // 絵は DirectComposition が出す。無効域を消すだけ。
        ValidateRect(window, nullptr);
        return 0;
    case WM_NCDESTROY:
        forget(window);
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, word, data);
}
} // namespace nenenib::ui::win32
