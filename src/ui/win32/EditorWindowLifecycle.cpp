#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "DevicePixels.hpp"
#include "EditorSettings.hpp"
#include "EndSession.hpp"
#include "ExitCause.hpp"
#include "ExitPlan.hpp"
#include "Milestone.hpp"
#include "RgbColor.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarWidth.hpp"
#include "UnsavedTab.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace nenenib::ui::win32
{
using detail::font_of;
using detail::widen;

namespace
{
constexpr wchar_t class_name[] = L"NeNeNib.Editor";

// DWM の COLORREF は 0x00BBGGRR（#795・ADR 0008 の決定 9 の追記）。
[[nodiscard]] constexpr COLORREF colorref_of(core::RgbColor color) noexcept
{
    constexpr unsigned green_shift = 8U;
    constexpr unsigned blue_shift = 16U;
    return static_cast<COLORREF>(color.red) | (static_cast<COLORREF>(color.green) << green_shift) |
           (static_cast<COLORREF>(color.blue) << blue_shift);
}

constexpr wchar_t title_suffix[] = L" - NeNe Nib";

constexpr std::int32_t design_width_dips = 640;
constexpr std::int32_t design_height_dips = 360;

// 裏の仕事の「届いた」の合図（ADR 0062 の決定 7）。番号を持つのはここだけ。
constexpr UINT work_message = WM_APP + 1;

// 起動の問いを最初の frame の後に出す、自分宛ての合図（ADR 0137 の決定 6）。
constexpr UINT autosave_question_message = WM_APP + 2;
} // namespace

EditorWindow::EditorWindow(HINSTANCE instance, application::EditorController &controller,
                           application::TimingPort &timing, RendererSources sources)
    : instance_(instance), controller_(controller), timing_(timing),
      text_layouts_(sources.text_layouts), warmup_(sources.warmup)
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
EditorWindow::create(HINSTANCE instance, application::EditorController &controller,
                     application::TimingPort &timing, RendererSources sources)
{
    auto window =
        std::unique_ptr<EditorWindow>(new EditorWindow(instance, controller, timing, sources));
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
    registration.style = CS_DBLCLKS; // 面のダブルクリック（ADR 0162）。面の外は押しと同じ。
    class_ = RegisterClassExW(&registration);
    if (class_ == 0)
    {
        return std::unexpected(WindowFailure::class_registration);
    }
    // 枠は WM_NCCALCSIZE で消すが、Snap・影・最小化の動きのために WS_THICKFRAME を残す（ADR
    // 0008）。 WS_EX_NOREDIRECTIONBITMAP: 再描画面を持たないので DirectComposition
    // のアルファがそのまま通る。
    window_ =
        CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_ACCEPTFILES, class_name, L"NeNe Nib",
                        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, design_width_dips,
                        design_height_dips, nullptr, nullptr, instance_, nullptr);
    if (window_ == nullptr)
    {
        return std::unexpected(WindowFailure::creation);
    }
    // 窓プロシージャから this に戻る経路はここだけ（GWLP_USERDATA）。
    SetWindowLongPtrW(window_, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));
    timing_.mark(core::Milestone::window_created);
    dpi_ = GetDpiForWindow(window_);
    read_wheel_setting();
    place_at_screen_centre();
    // 起動引数の結果を先に控える。最初の描画が出す VisibleLines の意図で last_failure は消える
    // （ADR 0010 の決定 9）。
    const auto opened = controller_.delivery();
    apply_backdrop(opened);
    shown_font_ = font_of(opened.settings);
    timing_.mark(core::Milestone::backdrop_applied);
    // 配置してから見せる。生成時に (0,0) で見せない（ADR 0008 の決定 7）。device の生成は
    // ドライバの初期化で 160 ms 掛かるので、その前に Mica の面だけの窓を見せる（ADR 0013）。
    ShowWindow(window_, SW_SHOW);
    timing_.mark(core::Milestone::window_shown);
    // 最初のフレームまで renderer_ は無い。来た入力は門が捨てる（ADR 0013 の決定 5）。
    const auto rendering = start_rendering();
    if (!rendering)
    {
        return rendering;
    }
    // 題名は起動引数で開いた文書にも追従する（ADR 0010 の決定 13）。
    update_title(controller_.delivery());
    // 開けなかった理由も 1 行出す。窓が出てから出すので、利用者は空の無題で作業を続けられる。
    announce(opened);
    announce_settings(opened);
    // 起動の問い（ADR 0140 の決定 2）。答えを待つ間の frame
    // は本文を描かない（display_pending）ので、 最初の frame
    // は本文の文字組みを作らずに出ている。印は最初の frame の意図が消すので、起動の delivery
    // で控えた値を読む。
    finish_display_question(opened.display_question);
    start_signal_timers();
    // 起動の問いは最初の frame の後、メッセージの配りが始まってから（決定 6）。
    static_cast<void>(PostMessageW(window_, autosave_question_message, 0, 0));
    return {};
}

void EditorWindow::apply_backdrop(const application::EditorDelivery &delivery)
{
    // Mica の明暗は DWM が持つので、表示値の外観をそのまま伝える。色は渡さない（ADR 0008）。
    shown_appearance_ = delivery.appearance;
    const BOOL dark = delivery.appearance == core::Appearance::dark ? TRUE : FALSE;
    DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                          static_cast<DWORD>(sizeof(dark)));
    // 窓の縁（角の 1 px）は帯の地 title_bar で塗る（#795）。最大化では OS が縁を描かない。
    // Windows 10 には属性が無く失敗を返すだけなので、Mica と同じく戻り値は見ない。
    shown_border_ = core::selected_theme(delivery.settings, delivery.appearance).ui.title_bar;
    const COLORREF border = colorref_of(shown_border_);
    DwmSetWindowAttribute(window_, DWMWA_BORDER_COLOR, &border, static_cast<DWORD>(sizeof(border)));
    // Mica は Windows 11 22H2 以降。掛からない環境でも帯は不透明の title_bar で塗るので（D16）、
    // 掛かったかどうかは誰も読まない。2 属性は最初のフレームまでの面（ADR 0013）と
    // 非クライアントの明暗（ADR 0008 の決定 9）のために掛け続ける。
    constexpr DWM_SYSTEMBACKDROP_TYPE mica = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(window_, DWMWA_SYSTEMBACKDROP_TYPE, &mica,
                          static_cast<DWORD>(sizeof(mica)));
}

std::expected<void, WindowFailure> EditorWindow::start_rendering()
{
    auto renderer = create_renderer();
    if (!renderer)
    {
        return std::unexpected(WindowFailure::render);
    }
    renderer_ = std::make_unique<Direct2DRenderer>(std::move(renderer).value());
    // 帯の幅は最初の描画の前に知らせる（アクティブなタブが見える送り量に直る・ADR 0056 の決定 8）。
    static_cast<void>(controller_.apply(application::TitleBarWidth{title_bar_width()}));
    static_cast<void>(controller_.apply(body_view()));
    if (!draw_frame())
    {
        return std::unexpected(WindowFailure::render);
    }
    // 本窓に描けたら、仮の renderer の残した子窓（作れなかった・device lost の後）を外す。
    if (!renderer_->provisional())
    {
        cover_.reset();
    }
    return {};
}

// ADR 0132 の決定 2: 本物の device がまだなら WARP で子窓に描き、本窓に WARP を結ばない。
// 本物が出来ていれば（device lost の作り直し・遅い起動）、今日どおり本窓に作る。
std::expected<Direct2DRenderer, RenderFailure> EditorWindow::create_renderer()
{
    if (!warmup_.ready())
    {
        auto covered = create_provisional_renderer();
        if (covered)
        {
            return covered;
        }
    }
    return Direct2DRenderer::create(window_, dpi_, timing_, {text_layouts_, warmup_});
}

std::expected<Direct2DRenderer, RenderFailure> EditorWindow::create_provisional_renderer()
{
    if (cover_ == nullptr)
    {
        auto cover = CoverWindow::create(instance_, window_);
        if (!cover)
        {
            return std::unexpected(RenderFailure::device_creation);
        }
        cover_ = std::move(cover).value();
        // 鍵は本窓に届ける（決定 5）。子窓が焦点を取っていたら戻す。
        if (GetFocus() == cover_->handle())
        {
            SetFocus(window_);
        }
    }
    return Direct2DRenderer::create_provisional(cover_->handle(), dpi_, timing_,
                                                {text_layouts_, warmup_});
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
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_MOUSEWHEEL:
    case WM_CAPTURECHANGED:
        return pointer_message(message, word, data);
    case WM_GETMINMAXINFO:
    case WM_SETTINGCHANGE:
    case WM_SIZE:
    case WM_DPICHANGED:
        return frame_message(message, word, data);
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint();
        return 0;
    case WM_DROPFILES:
        open_dropped_files(word);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_KILLFOCUS:
        return key_message(message, word, data);
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
        return compose_message(message, word, data);
    case WM_CLOSE:
    case WM_QUERYENDSESSION:
    case WM_ENDSESSION:
    case WM_DESTROY:
    case WM_TIMER:
    case work_message:
    case autosave_question_message:
        return lifetime_message(message, word, data);
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

LRESULT EditorWindow::lifetime_message(UINT message, WPARAM word, LPARAM data)
{
    // 窓の終わりの 4 通と、裏の仕事の「届いた」の合図（ADR 0062 の決定 7）と、自動退避の合図
    // （ADR 0137 の決定 1・6）。
    switch (message)
    {
    case WM_CLOSE:
        close_window();
        return 0;
    case WM_QUERYENDSESSION:
        // ENDSESSION_CRITICAL でも同じに答える。止められるかは OS 次第（ADR 0133 の限界）。
        return query_session_end();
    case WM_ENDSESSION:
        // OS の終了が確定したら（word が真）一覧を書く。窓は壊さない（ADR 0059 の決定 3）。
        if (word != FALSE)
        {
            // 確かめずに閉じるので、捨てた印は立てない（未保存の退避は残る・ADR 0137 の決定 5）。
            remember_session(application::EndSession{application::SessionEnd::window_closed});
        }
        return 0;
    case WM_DESTROY:
        end_selection_drag();
        KillTimer(window_, autosave_timer);
        KillTimer(window_, language_server_timer);
        stop_preview_timer();
        PostQuitMessage(0);
        return 0;
    case WM_TIMER:
        receive_timer(word);
        return 0;
    case work_message:
        receive_work();
        return 0;
    case autosave_question_message:
        offer_autosave();
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

void EditorWindow::limit_size(LPARAM data) const noexcept
{
    // 窓の最小の大きさは core が決める（ADR 0056 の決定 8）。窓はそれを OS へ写すだけ。
    auto *limits = std::bit_cast<MINMAXINFO *>(data);
    const auto minimum = core::minimum_window(dpi_);
    limits->ptMinTrackSize.x = core::width_of(minimum);
    limits->ptMinTrackSize.y = core::height_of(minimum);
}

// 窓が壊れるまで張る 30 秒ごとの合図（張れなければ合図が来ないだけ）。自動退避（ADR 0137 の
// 決定 1）と言語サーバーの古さ（ADR 0160 の決定 6）。
void EditorWindow::start_signal_timers()
{
    static_cast<void>(SetTimer(window_, autosave_timer, autosave_interval_milliseconds, nullptr));
    static_cast<void>(
        SetTimer(window_, language_server_timer, language_server_interval_milliseconds, nullptr));
}

std::function<void()> EditorWindow::work_signal() const
{
    const HWND window = window_;
    // 戻り値は見ない。壊れた窓への post は失敗するだけで、合図を落としてよい（決定 7）。
    return [window] { static_cast<void>(PostMessageW(window, work_message, 0, 0)); };
}

void EditorWindow::invalidate() noexcept
{
    if (window_ != nullptr && !paint_pending_)
    {
        // 失敗した要求は覚えず、次の変更で同じ入口から再要求する。
        paint_pending_ = InvalidateRect(window_, nullptr, FALSE) != FALSE;
    }
}

void EditorWindow::paint()
{
    // 無効領域を先に消してから描く。描いている間に来た変更は次の WM_PAINT が拾う。
    ValidateRect(window_, nullptr);
    // 描画中の再要求を落とさないよう、present より前に次の無効化を受け付ける。
    paint_pending_ = false;
    present();
}

void EditorWindow::update_title(const application::EditorDelivery &delivery)
{
    std::wstring title = widen(delivery.document.title.text()) + title_suffix;
    if (title == window_title_)
    {
        return;
    }
    window_title_ = std::move(title);
    SetWindowTextW(window_, window_title_.c_str());
}

void EditorWindow::close_window()
{
    // 描画を失った後の終わりは abandon が持つ。その確認の間に届いた WM_CLOSE は見送る。
    if (rendering_failed_)
    {
        return;
    }
    end_window(application::ExitCause::user_close, application::SessionEnd::window_closed);
}

void EditorWindow::end_window(application::ExitCause cause, application::SessionEnd reason)
{
    const auto unsaved = application::unsaved_tab_count(controller_.documents());
    switch (application::exit_plan(cause, unsaved))
    {
    case application::ExitPlan::leave_now:
        break;
    case application::ExitPlan::confirm_each_tab:
        // 取り消しか保存の失敗でそこで止め、窓を閉じない。それまでに保存したタブは保存されたまま。
        if (!confirm_unsaved_tabs(&EditorWindow::confirm_discard))
        {
            return;
        }
        break;
    case application::ExitPlan::confirm_each_tab_without_cancel:
        if (!confirm_unsaved_tabs(&EditorWindow::confirm_lost_tab))
        {
            return;
        }
        break;
    case application::ExitPlan::block_session_end:
        // OS の終了を止めるのは WM_QUERYENDSESSION の受け手（#493）。窓は続ける。
        return;
    }
    leave(reason);
}

void EditorWindow::leave(application::SessionEnd reason)
{
    // 来る道（end_window と最後のタブの close_tab）は未保存のタブをすべて確かめ済み。ここで残る
    // 未保存は使う人が「いいえ」と答えて捨てたもの（ADR 0137 の決定 5）。
    remember_session(application::EndSession{
        .reason = reason,
        .unsaved_discarded =
            application::next_unsaved_tab(controller_.documents(), 0).has_value()});
    // 壊す窓に立てた OS の終了を止める理由は、ここで外す（ADR 0133 の決定 4）。確認を通り抜けたので
    // 残る未保存は捨てると決めたもの。
    if (shutdown_blocked_)
    {
        static_cast<void>(ShutdownBlockReasonDestroy(window_));
        shutdown_blocked_ = false;
    }
    DestroyWindow(window_);
}

LRESULT EditorWindow::query_session_end()
{
    const auto unsaved = application::unsaved_tab_count(controller_.documents());
    switch (application::exit_plan(application::ExitCause::session_end, unsaved))
    {
    case application::ExitPlan::block_session_end:
        // OS が「終了を妨げています」と理由を見せる。使う人が戻ってから今日の確認を通す。
        // 立てた理由が残っている間は印を落とさない（2 回目の失敗で OS 側の理由が外れ残らない）。
        shutdown_blocked_ =
            shutdown_blocked_ ||
            ShutdownBlockReasonCreate(window_, L"保存されていない変更があります") != FALSE;
        return FALSE;
    case application::ExitPlan::leave_now:
        release_session_block();
        return TRUE;
    case application::ExitPlan::confirm_each_tab:
    case application::ExitPlan::confirm_each_tab_without_cancel:
        // session_end はこの 2 つを返さない（--exit-plan）。返ったら止めるほうへ倒す。
        return FALSE;
    }
    std::unreachable();
}

void EditorWindow::release_session_block()
{
    if (!shutdown_blocked_ || window_ == nullptr ||
        application::unsaved_tab_count(controller_.documents()) != 0)
    {
        return;
    }
    static_cast<void>(ShutdownBlockReasonDestroy(window_));
    shutdown_blocked_ = false;
}

void EditorWindow::remember_session(const application::EndSession &intent)
{
    // send を通さない。send の後半（IME・題名・告知・無効化）は壊れていく窓に触れるので、
    // 一覧を書くだけの意図には要らない（書けなくても何も出さない・ADR 0059 の決定 3）。
    static_cast<void>(controller_.apply(intent));
}

void EditorWindow::change_dpi(WPARAM word, LPARAM data)
{
    dpi_ = static_cast<UINT>((word >> 16U) & 0xFFFFU);
    const auto *suggested = std::bit_cast<const RECT *>(data);
    SetWindowPos(window_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    if (renderer_ != nullptr && !renderer_->set_dpi(dpi_) && !recover())
    {
        abandon();
        return;
    }
    // 物理画素の大きさが変わらなくても DIP の幅は変わる（ADR 0056 の決定 8）。
    send(application::TitleBarWidth{title_bar_width()});
    send(body_view());
    invalidate();
}

std::expected<void, RenderFailure> EditorWindow::draw_frame()
{
    text_layouts_.begin_frame();
    const auto frame = controller_.frame();
    // 打鍵の道の段（#844）。frame を作り終えた直後・描く前。
    timing_.mark(core::Milestone::frame_built);
    const auto drawn =
        frame.layout_failure.has_value()
            ? std::expected<void, RenderFailure>{std::unexpected(RenderFailure::directwrite)}
            : renderer_->render(frame, timing_);
    text_layouts_.end_frame();
    if (drawn)
    {
        presented_.presented(frame.display_pending, timing_);
        if (application::composing(frame))
        {
            place_candidate_window();
        }
    }
    return drawn;
}

void EditorWindow::present()
{
    if (renderer_ == nullptr)
    {
        return;
    }
    const auto drawn = draw_frame();
    if (drawn)
    {
        return;
    }
    // device lost だけは作り直して 1 回だけやり直す。それ以外と 2 度目の失敗は終わりへ（ADR 0007 /
    // ADR 0133）。recover は古い renderer を先に解放する。同じ HWND に DComp の target を 2 つ
    // 作らない（ADR 0132 の決定 6）。
    if (drawn.error() != RenderFailure::device_lost || !recover())
    {
        abandon();
    }
}

bool EditorWindow::provisional() const noexcept
{
    return renderer_ != nullptr && renderer_->provisional();
}

// ADR 0132 の決定 3: 本窓に本物の renderer を作って同じ frame を出し、その Commit が
// DWM に取り込まれ、DWM の次の合成が終わるのを待ってから、仮の renderer を解放して
// 子窓を壊す（子窓が消えた後に Mica が見える余地を塞ぐ）。本窓に作れなければ終了へ
// （device lost の 2 度目と同じ・ADR 0007）。
void EditorWindow::adopt_device()
{
    if (!provisional())
    {
        return;
    }
    auto adopted = Direct2DRenderer::create(window_, dpi_, timing_, {text_layouts_, warmup_});
    if (!adopted)
    {
        abandon();
        return;
    }
    auto provisional_renderer =
        std::exchange(renderer_, std::make_unique<Direct2DRenderer>(std::move(adopted).value()));
    const auto drawn = draw_frame();
    if (drawn)
    {
        // 待てなくても差し替えは続ける（子窓を壊すのが 1 frame 早まるだけ）。Commit の取り込みの
        // 後に DWM の次の合成も待つ。Commit だけでは子窓が消えた直後の 1 frame に Mica が見えた
        // （verify-window の差し替えの標本・ADR 0132 の限界）。
        static_cast<void>(renderer_->wait_for_commit());
        static_cast<void>(DwmFlush());
        timing_.mark(core::Milestone::device_adopted);
    }
    provisional_renderer.reset();
    cover_.reset();
    if (!drawn && (drawn.error() != RenderFailure::device_lost || !recover()))
    {
        abandon();
    }
}

bool EditorWindow::recover()
{
    // 古い renderer（swap chain と device）を先に手放してから作る。2 つを同時に持たない。
    renderer_.reset();
    return start_rendering().has_value();
}

bool EditorWindow::survives_layout(const application::EditorDelivery &delivery)
{
    if (!delivery.layout_failure.has_value())
    {
        return true;
    }
    switch (delivery.layout_failure.value())
    {
    case application::TextLayoutFailure::inconsistent:
        // 問いの往復の契約の破れは作り直しでは直らない。鍵は拒否済みで知らせは application が
        // 立てたので、作り直さずに残りを写す（ADR 0133 の追補 2026-10-18）。
        return true;
    case application::TextLayoutFailure::unavailable:
        break;
    }
    // 文字組みの失敗も 1 回だけ作り直し、作り直せたら意図の残りを写す（ADR 0133）。描画を失った後の
    // 終わりの途中（保存）では作り直さず、保存の失敗の理由を見せるために残りを写す。
    if (rendering_failed_ || recover())
    {
        return true;
    }
    abandon();
    return false;
}

void EditorWindow::abandon()
{
    // 終わりの確認の中（保存・タブの切り替え）で届いた失敗は、いま進んでいる終わりに任せる。
    if (rendering_failed_)
    {
        return;
    }
    rendering_failed_ = true;
    renderer_.reset();
    if (window_ == nullptr)
    {
        return;
    }
    end_window(application::ExitCause::rendering_lost, application::SessionEnd::rendering_lost);
}

bool EditorWindow::rendering_failed() const noexcept
{
    return rendering_failed_;
}

HWND EditorWindow::handle() const noexcept
{
    return window_;
}
} // namespace nenenib::ui::win32
