#include "EditorWindow.hpp"

#include "DevicePixels.hpp"
#include "EditorIntent.hpp"
#include "StatusBarLayout.hpp"
#include "TitleBarLayout.hpp"

#include <dwmapi.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr wchar_t class_name[] = L"NeNeNib.Editor";
constexpr std::int32_t design_width_dips = 640;
constexpr std::int32_t design_height_dips = 360;
constexpr std::int32_t resize_border_dips = 8;
constexpr std::size_t single_tab = 1;
// 縁のヒットテストは 3 行 3 列の表で引く。分岐で書くと CPP-012 の認知的複雑度を越える。
constexpr std::array<LRESULT, 9> border_codes{HTNOWHERE, HTLEFT,       HTRIGHT,
                                              HTTOP,     HTTOPLEFT,    HTTOPRIGHT,
                                              HTBOTTOM,  HTBOTTOMLEFT, HTBOTTOMRIGHT};

[[nodiscard]] std::int32_t low_word_of(LPARAM data) noexcept
{
    return static_cast<std::int32_t>(static_cast<short>(LOWORD(data)));
}

[[nodiscard]] std::int32_t high_word_of(LPARAM data) noexcept
{
    return static_cast<std::int32_t>(static_cast<short>(HIWORD(data)));
}

[[nodiscard]] bool caption_button(WPARAM word) noexcept
{
    return word == HTMINBUTTON || word == HTMAXBUTTON || word == HTCLOSE;
}

[[nodiscard]] bool window_button(core::TitleBarHit hit) noexcept
{
    return hit == core::TitleBarHit::minimize || hit == core::TitleBarHit::maximize ||
           hit == core::TitleBarHit::close;
}

[[nodiscard]] LRESULT border_hit(POINT point, const RECT &client, std::int32_t margin) noexcept
{
    const std::size_t row = point.y < margin ? 1U : (point.y >= client.bottom - margin ? 2U : 0U);
    const std::size_t column = point.x < margin ? 1U : (point.x >= client.right - margin ? 2U : 0U);
    return border_codes.at(row * 3U + column);
}

[[nodiscard]] LRESULT caption_code(core::TitleBarHit hit) noexcept
{
    switch (hit)
    {
    case core::TitleBarHit::minimize:
        return HTMINBUTTON;
    case core::TitleBarHit::maximize:
        return HTMAXBUTTON;
    case core::TitleBarHit::close:
        return HTCLOSE;
    case core::TitleBarHit::caption:
        return HTCAPTION;
    case core::TitleBarHit::tab:
    case core::TitleBarHit::add_tab:
    case core::TitleBarHit::none:
        return HTCLIENT;
    }
    std::unreachable();
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
    // 枠は WM_NCCALCSIZE で消すが、Snap・影・最小化の動きのために WS_THICKFRAME を残す（ADR
    // 0008）。 WS_EX_NOREDIRECTIONBITMAP: 再描画面を持たないので DirectComposition
    // のアルファがそのまま通る。
    window_ = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, class_name, L"NeNe Nib",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, design_width_dips,
                              design_height_dips, nullptr, nullptr, instance_, nullptr);
    if (window_ == nullptr)
    {
        return std::unexpected(WindowFailure::creation);
    }
    // 窓プロシージャから this に戻る経路はここだけ（GWLP_USERDATA）。
    SetWindowLongPtrW(window_, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));
    dpi_ = GetDpiForWindow(window_);
    place_at_screen_centre();
    apply_backdrop(controller_.frame());
    const auto rendering = start_rendering();
    if (!rendering)
    {
        return rendering;
    }
    // 配置してから見せる。生成時に (0,0) で見せない（ADR 0008 の決定 7）。
    ShowWindow(window_, SW_SHOW);
    return {};
}

void EditorWindow::apply_backdrop(const application::EditorFrame &frame)
{
    // Mica の明暗は DWM が持つので、表示値の外観をそのまま伝える。色は渡さない（ADR 0008）。
    const BOOL dark = frame.appearance == core::Appearance::dark ? TRUE : FALSE;
    const HRESULT themed = DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                                                 static_cast<DWORD>(sizeof(dark)));
    // Mica は Windows 11 22H2 以降。掛からない環境は結果で分かるので例外にしない（ARC-010）。
    constexpr DWM_SYSTEMBACKDROP_TYPE mica = DWMSBT_MAINWINDOW;
    const HRESULT applied = DwmSetWindowAttribute(window_, DWMWA_SYSTEMBACKDROP_TYPE, &mica,
                                                  static_cast<DWORD>(sizeof(mica)));
    backdrop_ =
        SUCCEEDED(themed) && SUCCEEDED(applied) ? TitleBarBackdrop::mica : TitleBarBackdrop::opaque;
}

std::expected<void, WindowFailure> EditorWindow::start_rendering()
{
    auto renderer = Direct2DRenderer::create(window_, dpi_);
    if (!renderer)
    {
        return std::unexpected(WindowFailure::render);
    }
    renderer_ = std::make_unique<Direct2DRenderer>(std::move(renderer).value());
    renderer_->set_backdrop(backdrop_);
    if (!renderer_->render(controller_.frame()))
    {
        return std::unexpected(WindowFailure::render);
    }
    return {};
}

void EditorWindow::place_at_screen_centre()
{
    const auto width = core::to_pixels(design_width_dips, dpi_);
    const auto height = core::to_pixels(design_height_dips, dpi_);
    MONITORINFO monitor{};
    monitor.cbSize = static_cast<DWORD>(sizeof(monitor));
    RECT work{0, 0, width, height};
    if (GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor) != 0)
    {
        work = monitor.rcWork;
    }
    const LONG left = work.left + (work.right - work.left - width) / 2;
    const LONG top = work.top + (work.bottom - work.top - height) / 2;
    SetWindowPos(window_, nullptr, left, top, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
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
    case WM_NCCALCSIZE:
        return calculate_client(word, data);
    case WM_NCACTIVATE:
        // 自分で答えて既定処理をさせない。非アクティブ化で OS に枠を描かせない（Folio ADR 0014）。
        return TRUE;
    case WM_NCPAINT:
        return 0;
    case WM_NCHITTEST:
        return hit_test(data);
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
        return press_caption(message, word, data);
    case WM_LBUTTONDOWN:
        click_client(data);
        return 0;
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

LRESULT EditorWindow::calculate_client(WPARAM word, LPARAM data) noexcept
{
    if (word != TRUE)
    {
        return DefWindowProcW(window_, WM_NCCALCSIZE, word, data);
    }
    // 枠を 0 にして client を窓全体に広げる。最大化のときだけ縁の分を内側へ寄せる（ADR 0008）。
    if (IsZoomed(window_) != 0)
    {
        auto *parameters = std::bit_cast<NCCALCSIZE_PARAMS *>(data);
        const int border = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi_) +
                           GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
        parameters->rgrc[0].left += border;
        parameters->rgrc[0].top += border;
        parameters->rgrc[0].right -= border;
        parameters->rgrc[0].bottom -= border;
    }
    return 0;
}

LRESULT EditorWindow::hit_test(LPARAM data) noexcept
{
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    RECT client{};
    GetClientRect(window_, &client);
    const auto layout = core::title_bar_layout(client.right, dpi_, single_tab);
    const auto hit = core::title_bar_hit(layout, point.x, point.y);
    // 窓の操作の上では大きさを変えられない。それ以外の縁は 8 DIP を 8 方向に割り当てる。
    if (window_button(hit))
    {
        return caption_code(hit);
    }
    const LRESULT border = border_hit(point, client, core::to_pixels(resize_border_dips, dpi_));
    if (border != HTNOWHERE)
    {
        return border;
    }
    return caption_code(hit);
}

LRESULT EditorWindow::press_caption(UINT message, WPARAM word, LPARAM data) noexcept
{
    if (!caption_button(word))
    {
        return DefWindowProcW(window_, message, word, data);
    }
    // 押下は飲み込み、離したときに動かす。DefWindowProcW に任せると OS の描画が混ざる。
    if (message == WM_NCLBUTTONUP)
    {
        activate_caption(word);
    }
    return 0;
}

void EditorWindow::activate_caption(WPARAM word) noexcept
{
    if (word == HTMINBUTTON)
    {
        ShowWindow(window_, SW_MINIMIZE);
    }
    if (word == HTMAXBUTTON)
    {
        ShowWindow(window_, IsZoomed(window_) != 0 ? SW_RESTORE : SW_MAXIMIZE);
    }
    if (word == HTCLOSE)
    {
        DestroyWindow(window_);
    }
}

void EditorWindow::click_client(LPARAM data)
{
    RECT client{};
    GetClientRect(window_, &client);
    const auto layout = core::status_bar_layout(client.right, client.bottom, dpi_);
    switch (core::status_bar_hit(layout, low_word_of(data), high_word_of(data)))
    {
    // 意図は「どちらを選んだか」。同じ側を押しても controller が同じ表示値を返すだけ（ARC-011）。
    case core::StatusBarHit::toggle_ordinary:
        present(controller_.apply(application::EditorIntent::select_ordinary_mode));
        return;
    case core::StatusBarHit::toggle_vim:
        present(controller_.apply(application::EditorIntent::select_vim_mode));
        return;
    case core::StatusBarHit::none:
        return;
    }
}

void EditorWindow::refresh_appearance()
{
    // 状態遷移は controller だけが行い、窓は返ってきた表示値を写す（ARC-011）。
    const auto frame = controller_.apply(application::EditorIntent::refresh_appearance);
    apply_backdrop(frame);
    if (renderer_ != nullptr)
    {
        renderer_->set_backdrop(backdrop_);
    }
    present(frame);
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
    if (renderer_ != nullptr && !renderer_->set_dpi(dpi_))
    {
        abandon();
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
