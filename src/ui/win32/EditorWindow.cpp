#include "EditorWindow.hpp"

#include "EditorIntent.hpp"

#include <bit>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr wchar_t class_name[] = L"NeNeNib.Editor";
constexpr int design_width_dips = 640;
constexpr int design_height_dips = 360;
constexpr UINT reference_dpi = 96;

[[nodiscard]] int scaled(int dips, UINT dpi) noexcept
{
    return dips * static_cast<int>(dpi) / static_cast<int>(reference_dpi);
}

[[nodiscard]] float dpi_scale(UINT dpi) noexcept
{
    return static_cast<float>(dpi);
}
} // namespace

EditorWindow::EditorWindow(HINSTANCE instance, application::EditorController &controller)
    : instance_(instance), controller_(controller)
{
}

EditorWindow::~EditorWindow()
{
    renderer_.reset();
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }
    if (class_ != 0)
    {
        UnregisterClassW(class_name, instance_);
    }
}

std::expected<std::unique_ptr<EditorWindow>, WindowFailure>
EditorWindow::create(HINSTANCE instance, application::EditorController &controller)
{
    auto window = std::unique_ptr<EditorWindow>(new EditorWindow(instance, controller));
    const auto ready = window->initialize();
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    return window;
}

std::expected<void, WindowFailure> EditorWindow::initialize()
{
    WNDCLASSEXW registration{};
    registration.cbSize = static_cast<UINT>(sizeof(registration));
    registration.hInstance = instance_;
    registration.lpfnWndProc = procedure;
    registration.lpszClassName = class_name;
    registration.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    class_ = RegisterClassExW(&registration);
    if (class_ == 0)
    {
        return std::unexpected(WindowFailure::class_registration);
    }
    window_ =
        CreateWindowExW(0, class_name, L"NeNe Nib", WS_POPUP | WS_VISIBLE, 0, 0, design_width_dips,
                        design_height_dips, nullptr, nullptr, instance_, nullptr);
    if (window_ == nullptr)
    {
        return std::unexpected(WindowFailure::creation);
    }
    // 窓プロシージャから this に戻る経路はここだけ（GWLP_USERDATA）。
    SetWindowLongPtrW(window_, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));
    dpi_ = GetDpiForWindow(window_);
    place_at_screen_centre();
    return start_rendering();
}

std::expected<void, WindowFailure> EditorWindow::start_rendering()
{
    auto renderer = Direct2DRenderer::create(window_, dpi_scale(dpi_));
    if (!renderer)
    {
        return std::unexpected(WindowFailure::render);
    }
    renderer_ = std::make_unique<Direct2DRenderer>(std::move(renderer).value());
    if (!renderer_->render(controller_.frame()))
    {
        return std::unexpected(WindowFailure::render);
    }
    return {};
}

void EditorWindow::place_at_screen_centre()
{
    const int width = scaled(design_width_dips, dpi_);
    const int height = scaled(design_height_dips, dpi_);
    MONITORINFO monitor{};
    monitor.cbSize = static_cast<DWORD>(sizeof(monitor));
    RECT work{0, 0, width, height};
    if (GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor) != 0)
    {
        work = monitor.rcWork;
    }
    const int left = work.left + (work.right - work.left - width) / 2;
    const int top = work.top + (work.bottom - work.top - height) / 2;
    SetWindowPos(window_, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK EditorWindow::procedure(HWND window, UINT message, WPARAM word,
                                         LPARAM data) noexcept
{
    auto *self = std::bit_cast<EditorWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self == nullptr)
    {
        return DefWindowProcW(window, message, word, data);
    }
    const LRESULT result = self->dispatch(message, word, data);
    if (message == WM_NCDESTROY)
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        self->window_ = nullptr;
    }
    return result;
}

LRESULT EditorWindow::dispatch(UINT message, WPARAM word, LPARAM data) noexcept
{
    // OS のメッセージ番号は開いた集合なので、既定分岐だけは許される（CPP-017）。
    switch (message)
    {
    case WM_NCHITTEST:
        return HTCAPTION;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        ValidateRect(window_, nullptr);
        return 0;
    case WM_KEYDOWN:
        if (word == VK_ESCAPE)
        {
            DestroyWindow(window_);
        }
        return 0;
    case WM_SETTINGCHANGE:
        refresh_appearance();
        return 0;
    case WM_SIZE:
        resize();
        return 0;
    case WM_DPICHANGED:
        change_dpi(word, data);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

void EditorWindow::refresh_appearance()
{
    // 状態遷移は controller だけが行い、窓は返ってきた表示値を写す（ARC-011）。
    present(controller_.apply(application::EditorIntent::refresh_appearance));
}

void EditorWindow::resize()
{
    if (renderer_ == nullptr)
    {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    if (!renderer_->resize(static_cast<UINT>(client.right), static_cast<UINT>(client.bottom)))
    {
        abandon();
        return;
    }
    present(controller_.frame());
}

void EditorWindow::change_dpi(WPARAM word, LPARAM data)
{
    dpi_ = static_cast<UINT>((word >> 16U) & 0xFFFFU);
    const auto *suggested = std::bit_cast<const RECT *>(data);
    SetWindowPos(window_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    if (renderer_ != nullptr)
    {
        renderer_->set_dpi(dpi_scale(dpi_));
    }
}

void EditorWindow::present(const application::EditorFrame &frame)
{
    if (renderer_ == nullptr)
    {
        return;
    }
    const auto drawn = renderer_->render(frame);
    if (drawn)
    {
        return;
    }
    // device lost だけは作り直して 1 回だけやり直す。それ以外と 2 度目の失敗は終了へ（ADR 0007）。
    if (drawn.error() != RenderFailure::device_lost || !start_rendering())
    {
        abandon();
    }
}

void EditorWindow::abandon()
{
    rendering_failed_ = true;
    renderer_.reset();
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }
}

bool EditorWindow::rendering_failed() const noexcept
{
    return rendering_failed_;
}
} // namespace nenenib::ui::win32
