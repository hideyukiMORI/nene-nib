#include "EditorWindow.hpp"

#include "BodyLayout.hpp"
#include "CancelComposition.hpp"
#include "CaretMotion.hpp"
#include "ClauseEmphasis.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "EditorIntent.hpp"
#include "FileDialog.hpp"
#include "FileFailure.hpp"
#include "KeyMotion.hpp"
#include "KeyVimSpecial.hpp"
#include "Milestone.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "SelectionAnchoring.hpp"
#include "StatusBarLayout.hpp"
#include "TextEncoding.hpp"
#include "TitleBarLayout.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"

#include <dwmapi.h>
#include <imm.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
constexpr wchar_t class_name[] = L"NeNeNib.Editor";
constexpr wchar_t product_name[] = L"NeNe Nib";
constexpr wchar_t title_suffix[] = L" - NeNe Nib";
constexpr wchar_t untitled_file[] = L"無題.txt";
constexpr std::int32_t design_width_dips = 640;
constexpr std::int32_t design_height_dips = 360;
constexpr std::int32_t resize_border_dips = 8;
constexpr std::size_t single_tab = 1;
constexpr std::int32_t wheel_lines = 3;
constexpr SHORT key_down_mask = static_cast<SHORT>(0x8000);
constexpr wchar_t first_high_surrogate = 0xD800;
constexpr wchar_t first_low_surrogate = 0xDC00;
constexpr wchar_t last_low_surrogate = 0xDFFF;
constexpr wchar_t first_printable = 0x20;
constexpr wchar_t delete_character = 0x7F;
// キー → キャレットの移動は表で引く（CPP-012）。Ctrl の有無は表そのものを分けて表す。
constexpr std::array<KeyMotion, 8> plain_motions{{{VK_LEFT, core::CaretMotion::previous_character},
                                                  {VK_RIGHT, core::CaretMotion::next_character},
                                                  {VK_UP, core::CaretMotion::previous_line},
                                                  {VK_DOWN, core::CaretMotion::next_line},
                                                  {VK_HOME, core::CaretMotion::line_start},
                                                  {VK_END, core::CaretMotion::line_end},
                                                  {VK_PRIOR, core::CaretMotion::page_up},
                                                  {VK_NEXT, core::CaretMotion::page_down}}};
constexpr std::array<KeyMotion, 4> control_motions{{{VK_LEFT, core::CaretMotion::previous_word},
                                                    {VK_RIGHT, core::CaretMotion::next_word},
                                                    {VK_HOME, core::CaretMotion::document_start},
                                                    {VK_END, core::CaretMotion::document_end}}};
// Vim モードの仮想キー → 特別な鍵。通常モードの表と入れ替えて引く（ADR 0012 の決定 4）。
constexpr std::array<KeyVimSpecial, 9> vim_specials{{{VK_ESCAPE, core::VimSpecialKey::escape},
                                                     {VK_RETURN, core::VimSpecialKey::enter},
                                                     {VK_BACK, core::VimSpecialKey::backspace},
                                                     {VK_LEFT, core::VimSpecialKey::arrow_left},
                                                     {VK_RIGHT, core::VimSpecialKey::arrow_right},
                                                     {VK_UP, core::VimSpecialKey::arrow_up},
                                                     {VK_DOWN, core::VimSpecialKey::arrow_down},
                                                     {VK_HOME, core::VimSpecialKey::home},
                                                     {VK_END, core::VimSpecialKey::end}}};
constexpr char32_t tab_character = U'\t';

[[nodiscard]] bool held(int key) noexcept
{
    return (GetKeyState(key) & key_down_mask) != 0;
}

[[nodiscard]] std::optional<core::CaretMotion> motion_for(std::span<const KeyMotion> table,
                                                          WPARAM key) noexcept
{
    for (const KeyMotion entry : table)
    {
        if (entry.key == key)
        {
            return entry.motion;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<core::VimSpecialKey> vim_special_for(WPARAM key) noexcept
{
    for (const KeyVimSpecial entry : vim_specials)
    {
        if (entry.key == key)
        {
            return entry.special;
        }
    }
    return std::nullopt;
}

[[nodiscard]] core::SelectionAnchoring anchoring_now() noexcept
{
    return held(VK_SHIFT) ? core::SelectionAnchoring::extend : core::SelectionAnchoring::collapse;
}

// UTF-8 の表示値を Win32 の UTF-16 へ。題名とダイアログの既定名だけが通る（CPP-014）。
// 変換そのものは core::to_utf16 ただ 1 本で、表示値は検証済みなので失敗しない（Issue #13）。
[[nodiscard]] std::wstring widen(std::string_view utf8)
{
    return core::to_utf16(utf8).value_or(std::wstring{});
}

// 失敗の理由は 1 行だけ出す。本文は変わらない（ADR 0010 の決定 9）。
[[nodiscard]] const wchar_t *reason_of(application::FileFailure failure) noexcept
{
    switch (failure)
    {
    case application::FileFailure::not_found:
        return L"ファイルが見つかりませんでした。";
    case application::FileFailure::access_denied:
        return L"ファイルを開く権限がありません。";
    case application::FileFailure::unreadable:
        return L"ファイルを読み取れませんでした。";
    case application::FileFailure::unwritable:
        return L"ファイルを保存できませんでした。元のファイルは変わっていません。";
    case application::FileFailure::too_large:
        return L"64 MiB を超えるファイルはまだ開けません。";
    case application::FileFailure::undecodable:
        return L"文字コードを判別できませんでした。";
    case application::FileFailure::unencodable:
        return L"この文字コードでは保存できない文字があります。";
    }
    std::unreachable();
}

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

// クリックした y から見えている行の並びの何番目か。行より下は最も近い（最後の）行に寄せる。
[[nodiscard]] std::size_t row_index(const core::BodyLayout &body, std::int32_t y,
                                    std::size_t count) noexcept
{
    if (y <= body.content.top)
    {
        return 0;
    }
    const auto row = static_cast<std::size_t>((y - body.content.top) / body.line_height);
    return std::min(row, count - 1);
}

// ---------------------------------------------------------------- IMM32（ADR 0014）

// 長さを先に取ってからバッファに読む。ImmGetCompositionStringW はバイト数を返す。
[[nodiscard]] std::wstring composition_string(HIMC context, DWORD kind, DWORD flags)
{
    if ((flags & kind) == 0)
    {
        return {};
    }
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0)
    {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
    ImmGetCompositionStringW(context, kind, wide.data(), static_cast<DWORD>(bytes));
    return wide;
}

[[nodiscard]] std::vector<std::uint8_t> composition_bytes(HIMC context, DWORD kind)
{
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0)
    {
        return {};
    }
    std::vector<std::uint8_t> read(static_cast<std::size_t>(bytes), 0);
    ImmGetCompositionStringW(context, kind, read.data(), static_cast<DWORD>(bytes));
    return read;
}

// GCS_COMPCLAUSE は DWORD の並び（UTF-16 の単位で、先頭は 0・末尾は長さ）。バイト列から
// DWORD へ戻すのは std::bit_cast（reinterpret_cast は書けない・CPP-009）。
[[nodiscard]] std::vector<std::size_t> composition_boundaries(HIMC context)
{
    const auto bytes = composition_bytes(context, GCS_COMPCLAUSE);
    std::vector<std::size_t> boundaries;
    for (std::size_t at = 0; at + sizeof(DWORD) <= bytes.size(); at += sizeof(DWORD))
    {
        std::array<std::uint8_t, sizeof(DWORD)> word{};
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(at), word.size(), word.begin());
        boundaries.push_back(static_cast<std::size_t>(std::bit_cast<DWORD>(word)));
    }
    return boundaries;
}

// UTF-16 の単位の位置 → UTF-8 のバイト位置の表。文節の境界も属性も GCS_CURSORPOS も
// どれも UTF-16 の単位なので、写しはこの 1 本を通す（CPP-014 / Issue #13）。
[[nodiscard]] std::vector<std::size_t> byte_offsets(std::wstring_view wide)
{
    // 先頭は必ず 0 で始める（空の prefix を変換しに行かない）。
    std::vector<std::size_t> offsets{0};
    offsets.reserve(wide.size() + 1);
    for (std::size_t units = 1; units <= wide.size(); ++units)
    {
        // サロゲートの途中で切った prefix は変換できないので、直前の値を伸ばさずに使う。
        const auto converted = core::to_utf8(wide.substr(0, units));
        offsets.push_back(converted.has_value() ? converted.value().size() : offsets.back());
    }
    return offsets;
}

[[nodiscard]] std::size_t byte_at(const std::vector<std::size_t> &offsets, std::size_t unit)
{
    if (offsets.empty())
    {
        return 0;
    }
    return offsets.at(std::min(unit, offsets.size() - 1));
}

// 文節の強さは文節の先頭の属性が決める（ADR 0014 の決定 7）。属性の集合は IME が決める
// 開いた集合なので、閉じた enum に畳むのはここ 1 か所だけ。
[[nodiscard]] core::ClauseEmphasis emphasis_at(const std::vector<std::uint8_t> &attributes,
                                               std::size_t unit) noexcept
{
    if (unit >= attributes.size())
    {
        return core::ClauseEmphasis::other;
    }
    const std::uint8_t attribute = attributes.at(unit);
    if (attribute == ATTR_TARGET_CONVERTED || attribute == ATTR_TARGET_NOTCONVERTED)
    {
        return core::ClauseEmphasis::target;
    }
    return core::ClauseEmphasis::other;
}

[[nodiscard]] std::vector<core::CompositionClause>
clauses_of(const std::vector<std::size_t> &boundaries, const std::vector<std::uint8_t> &attributes,
           const std::vector<std::size_t> &offsets)
{
    std::vector<core::CompositionClause> clauses;
    for (std::size_t index = 0; index + 1 < boundaries.size(); ++index)
    {
        const std::size_t from = boundaries.at(index);
        const std::size_t to = boundaries.at(index + 1);
        clauses.push_back(
            core::CompositionClause{core::OffsetRange{core::Offset{byte_at(offsets, from)},
                                                      core::Offset{byte_at(offsets, to)}},
                                    emphasis_at(attributes, from)});
    }
    return clauses;
}

[[nodiscard]] core::Composition composition_of(const std::wstring &wide,
                                               const std::vector<std::uint8_t> &attributes,
                                               const std::vector<std::size_t> &boundaries,
                                               LONG cursor)
{
    const auto offsets = byte_offsets(wide);
    // 変換中の文字列は IME が作った正しい UTF-16 なので、空になるのは本当に空のときだけ。
    std::string utf8 = core::to_utf8(wide).value_or(std::string{});
    const std::size_t unit = cursor < 0 ? wide.size() : static_cast<std::size_t>(cursor);
    return core::Composition{std::move(utf8), clauses_of(boundaries, attributes, offsets),
                             core::Offset{byte_at(offsets, unit)}};
}

// GCS_COMPSTR が立っていなければ変換中の文字列は変わっていない（確定だけのメッセージ）。
[[nodiscard]] std::optional<core::Composition> composition_in(HIMC context, DWORD flags)
{
    if ((flags & GCS_COMPSTR) == 0)
    {
        return std::nullopt;
    }
    const std::wstring wide = composition_string(context, GCS_COMPSTR, flags);
    return composition_of(wide, composition_bytes(context, GCS_COMPATTR),
                          composition_boundaries(context),
                          ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0));
}

// Vim の NORMAL でだけ IME を切る。通常モードの開閉には触らない（ADR 0014 の決定 5）。
[[nodiscard]] bool ime_blocked(core::EditMode mode, core::VimMode vim) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return false;
    case core::EditMode::vim:
        break;
    }
    switch (vim)
    {
    case core::VimMode::insert:
        return false;
    case core::VimMode::normal:
        return true;
    }
    std::unreachable();
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

EditorWindow::EditorWindow(HINSTANCE instance, application::EditorController &controller,
                           application::TimingPort &timing)
    : instance_(instance), controller_(controller), timing_(timing)
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
                     application::TimingPort &timing)
{
    auto window = std::unique_ptr<EditorWindow>(new EditorWindow(instance, controller, timing));
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
    timing_.mark(core::Milestone::window_created);
    dpi_ = GetDpiForWindow(window_);
    place_at_screen_centre();
    // 起動引数の結果を先に控える。最初の描画が出す VisibleLines の意図で last_failure は消える
    // （ADR 0010 の決定 9）。
    const auto opened = controller_.frame();
    apply_backdrop(opened);
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
    update_title(controller_.frame());
    // 開けなかった理由も 1 行出す。窓が出てから出すので、利用者は空の無題で作業を続けられる。
    announce(opened);
    return {};
}

void EditorWindow::apply_backdrop(const application::EditorFrame &frame)
{
    // Mica の明暗は DWM が持つので、表示値の外観をそのまま伝える。色は渡さない（ADR 0008）。
    const BOOL dark = frame.appearance == core::Appearance::dark ? TRUE : FALSE;
    DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                          static_cast<DWORD>(sizeof(dark)));
    // Mica は Windows 11 22H2 以降。掛からない環境でも帯は不透明の title_bar で塗るので（D16）、
    // 掛かったかどうかは誰も読まない。2 属性は最初のフレームまでの面（ADR 0013）と
    // 非クライアントの明暗（ADR 0008 の決定 9）のために掛け続ける。
    constexpr DWM_SYSTEMBACKDROP_TYPE mica = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(window_, DWMWA_SYSTEMBACKDROP_TYPE, &mica,
                          static_cast<DWORD>(sizeof(mica)));
}

std::expected<void, WindowFailure> EditorWindow::start_rendering()
{
    auto renderer = Direct2DRenderer::create(window_, dpi_, timing_);
    if (!renderer)
    {
        return std::unexpected(WindowFailure::render);
    }
    renderer_ = std::make_unique<Direct2DRenderer>(std::move(renderer).value());
    if (!draw_frame(controller_.apply(application::VisibleLines{body_lines()})))
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
        paint();
        return 0;
    case WM_KEYDOWN:
        timing_.mark(core::Milestone::input_received);
        press_key(word);
        return 0;
    case WM_CHAR:
        timing_.mark(core::Milestone::input_received);
        type_character(word);
        return 0;
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
        return compose_message(message, word, data);
    case WM_MOUSEWHEEL:
        turn_wheel(word);
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
    case WM_CLOSE:
        close_window();
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
        // 閉じる経路は WM_CLOSE 1 本（ARC-001）。未保存の確認もそこで 1 度だけ起きる。
        SendMessageW(window_, WM_CLOSE, 0, 0);
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
        send(application::SelectEditMode{core::EditMode::ordinary});
        return;
    case core::StatusBarHit::toggle_vim:
        send(application::SelectEditMode{core::EditMode::vim});
        return;
    case core::StatusBarHit::none:
        break;
    }
    const auto body = core::body_layout(client.right, client.bottom, dpi_);
    if (core::contains(body.band, low_word_of(data), high_word_of(data)))
    {
        place_caret(data);
    }
}

void EditorWindow::place_caret(LPARAM data)
{
    if (renderer_ == nullptr)
    {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const auto body = core::body_layout(client.right, client.bottom, dpi_);
    const auto frame = controller_.frame();
    if (frame.lines.empty())
    {
        return;
    }
    // 行番号の欄や行より左のクリックは行頭に寄せる（最も近い位置）。
    const auto &line = frame.lines.at(row_index(body, high_word_of(data), frame.lines.size()));
    const auto column =
        renderer_->column_at(line.text, body, std::max(low_word_of(data), body.content.left));
    const auto anchoring =
        held(VK_SHIFT) ? core::SelectionAnchoring::extend : core::SelectionAnchoring::collapse;
    send(application::PlaceCaret{core::TextPosition{line.number, column}, anchoring});
}

void EditorWindow::refresh_appearance()
{
    // 状態遷移は controller だけが行い、窓は返ってきた表示値を写す（ARC-011）。
    const auto frame = controller_.apply(application::RefreshAppearance{});
    apply_backdrop(frame);
    invalidate();
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
    // 何行入るかは application が持つ。窓は寸法から数えた行数を意図として渡すだけ（ADR 0009）。
    send(application::VisibleLines{body_lines()});
}

std::size_t EditorWindow::body_lines() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::body_layout(client.right, client.bottom, dpi_).visible_lines;
}

// ---------------------------------------------------------------- IMM32（ADR 0014）

// IMM32 の 3 通（決定 3）。WM_IME_STARTCOMPOSITION と WM_IME_COMPOSITION は DefWindowProcW に
// 渡さない＝ IME の既定の変換窓を出さず、確定文字を WM_CHAR に流さない。WM_IME_ENDCOMPOSITION は
// 残った変換を捨ててから OS にも見せる。変換中の鍵は IME が食い、WM_KEYDOWN は VK_PROCESSKEY で
// 来る＝ vim_specials の表に無いので Vim の鍵にならない（決定 5・既存の形のまま）。
LRESULT EditorWindow::compose_message(UINT message, WPARAM word, LPARAM data)
{
    switch (message)
    {
    case WM_IME_STARTCOMPOSITION:
        place_candidate_window();
        return 0;
    case WM_IME_COMPOSITION:
        compose(data);
        return 0;
    // OS のメッセージ番号は開いた集合なので、既定分岐だけは許される（CPP-017）。
    default:
        break;
    }
    end_composition();
    return DefWindowProcW(window_, message, word, data);
}

void EditorWindow::compose(LPARAM data)
{
    const auto flags = static_cast<DWORD>(data);
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    // 確定と変換中は 1 通のメッセージに同居しうるので、文脈を離す前に両方を読む（決定 3）。
    const std::string committed =
        core::to_utf8(composition_string(context, GCS_RESULTSTR, flags)).value_or(std::string{});
    const auto composing = composition_in(context, flags);
    ImmReleaseContext(window_, context);
    if (!committed.empty())
    {
        send(application::CommitText{committed});
    }
    if (composing.has_value())
    {
        send_composition(composing.value());
    }
    place_candidate_window();
}

void EditorWindow::send_composition(const core::Composition &composition)
{
    // 空の変換文字列は「変換をやめた」の合図（決定 3）。
    if (composition.utf8.empty())
    {
        send(application::CancelComposition{});
        return;
    }
    send(application::ComposeText{composition});
}

void EditorWindow::end_composition()
{
    if (!controller_.frame().composition.has_value())
    {
        return;
    }
    send(application::CancelComposition{});
}

// 候補窓はキャレットの直下。ImmSetCompositionWindow は使わない（変換文字列は自前で描く・決定 6）。
void EditorWindow::place_candidate_window()
{
    if (renderer_ == nullptr)
    {
        return;
    }
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    const RECT caret = renderer_->caret_rectangle();
    CANDIDATEFORM form{};
    form.dwStyle = CFS_CANDIDATEPOS;
    form.ptCurrentPos = POINT{caret.left, caret.bottom};
    ImmSetCandidateWindow(context, &form);
    ImmReleaseContext(window_, context);
}

void EditorWindow::follow_ime(const application::EditorFrame &frame)
{
    if (ime_blocked(frame.mode, frame.vim_mode))
    {
        close_ime();
        return;
    }
    restore_ime();
}

void EditorWindow::close_ime()
{
    // 控えが在る＝すでに切ってある。切る前の値を上書きしない（決定 5）。
    if (ime_open_ != ImeOpenState::unrecorded)
    {
        return;
    }
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    ime_open_ = ImmGetOpenStatus(context) != FALSE ? ImeOpenState::open : ImeOpenState::closed;
    ImmSetOpenStatus(context, FALSE);
    ImmReleaseContext(window_, context);
}

void EditorWindow::restore_ime()
{
    if (ime_open_ == ImeOpenState::unrecorded)
    {
        return;
    }
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    ImmSetOpenStatus(context, ime_open_ == ImeOpenState::open ? TRUE : FALSE);
    ime_open_ = ImeOpenState::unrecorded;
    ImmReleaseContext(window_, context);
}

void EditorWindow::send(const application::EditorIntent &intent)
{
    const auto frame = controller_.apply(intent);
    follow_ime(frame);
    mode_ = frame.mode;
    // 描くのは WM_PAINT。まとめて来た入力はここで無効化だけ積まれ、1 フレームに畳まれる（決定 6）。
    invalidate();
    update_title(frame);
    announce(frame);
}

void EditorWindow::invalidate() noexcept
{
    if (window_ != nullptr)
    {
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void EditorWindow::paint()
{
    // 無効領域を先に消してから描く。描いている間に来た変更は次の WM_PAINT が拾う。
    ValidateRect(window_, nullptr);
    present(controller_.frame());
}

void EditorWindow::update_title(const application::EditorFrame &frame)
{
    std::wstring title = widen(frame.document.title.text()) + title_suffix;
    if (title == window_title_)
    {
        return;
    }
    window_title_ = std::move(title);
    SetWindowTextW(window_, window_title_.c_str());
}

void EditorWindow::announce(const application::EditorFrame &frame)
{
    if (!frame.document.last_failure.has_value())
    {
        return;
    }
    const auto failure = frame.document.last_failure.value();
    if (failure == application::FileFailure::unencodable && frame.document.path.has_value())
    {
        offer_utf8(frame.document.path.value());
        return;
    }
    MessageBoxW(window_, reason_of(failure), product_name, MB_OK | MB_ICONWARNING);
}

void EditorWindow::offer_utf8(const core::FilePath &path)
{
    const int answer = MessageBoxW(
        window_, L"この文字コードでは保存できない文字があります。UTF-8 で保存しますか。",
        product_name, MB_YESNO | MB_ICONWARNING);
    if (answer != IDYES)
    {
        return;
    }
    send(application::SaveDocument{path, core::TextEncoding::utf8});
}

void EditorWindow::open_document()
{
    if (!confirm_discard())
    {
        return;
    }
    const auto chosen = choose_file_to_open(window_);
    if (!chosen.has_value())
    {
        return;
    }
    send(application::OpenDocument{chosen.value()});
}

void EditorWindow::save_document()
{
    const auto frame = controller_.frame();
    if (!frame.document.path.has_value())
    {
        save_document_as();
        return;
    }
    send(application::SaveDocument{frame.document.path.value(), frame.document.encoding});
}

void EditorWindow::save_document_as()
{
    const auto frame = controller_.frame();
    const std::wstring suggested = frame.document.path.has_value()
                                       ? widen(frame.document.path.value().file_name())
                                       : std::wstring(untitled_file);
    const auto chosen = choose_file_to_save(window_, suggested);
    if (!chosen.has_value())
    {
        return;
    }
    send(application::SaveDocument{chosen.value(), frame.document.encoding});
}

bool EditorWindow::confirm_discard()
{
    if (controller_.frame().document.save_state == core::SaveState::saved)
    {
        return true;
    }
    const int answer = MessageBoxW(window_, L"変更が保存されていません。保存しますか。",
                                   product_name, MB_YESNOCANCEL | MB_ICONWARNING);
    if (answer == IDNO)
    {
        return true;
    }
    if (answer != IDYES)
    {
        return false;
    }
    // 保存に失敗したか取り消したときは、まだ未保存のままなので続けない。
    save_document();
    return controller_.frame().document.save_state == core::SaveState::saved;
}

void EditorWindow::close_window()
{
    if (!confirm_discard())
    {
        return;
    }
    DestroyWindow(window_);
}

void EditorWindow::type_character(WPARAM word)
{
    const auto unit = static_cast<wchar_t>(word);
    if (unit >= first_high_surrogate && unit < first_low_surrogate)
    {
        pending_high_surrogate_ = unit;
        return;
    }
    const wchar_t pending = std::exchange(pending_high_surrogate_, 0);
    const bool low = unit >= first_low_surrogate && unit <= last_low_surrogate;
    // 制御文字は WM_KEYDOWN が扱う。対になっていない下位サロゲートと一緒にここで捨てる。
    if (unit < first_printable || unit == delete_character || (low && pending == 0))
    {
        return;
    }
    std::wstring wide;
    if (low)
    {
        wide.push_back(pending);
    }
    wide.push_back(unit);
    // 合成した 1 文字を UTF-8 の意図へ（CPP-014 / ADR 0009 の決定 2）。変換は core::to_utf8
    // ただ 1 本で、対にならないサロゲートはここまでに捨ててあるので空にはならない（Issue #13）。
    std::string utf8 = core::to_utf8(wide).value_or(std::string{});
    switch (mode_)
    {
    case core::EditMode::vim:
        send(application::VimKeyPress{
            core::VimKey{core::VimCharacter{core::code_point_at(utf8, core::Offset{0})}}});
        return;
    case core::EditMode::ordinary:
        break;
    }
    send(application::InsertText{std::move(utf8)});
}

void EditorWindow::press_key(WPARAM word)
{
    if (held(VK_CONTROL))
    {
        press_control_key(word);
        return;
    }
    switch (mode_)
    {
    case core::EditMode::vim:
        press_vim_key(word);
        return;
    case core::EditMode::ordinary:
        break;
    }
    press_plain_key(word);
}

// Vim モードの窓は鍵を写すだけで、何が起きるかは知らない（ARC-011 / ADR 0012 の決定 4）。
// Tab は INSERT の水平タブとして文字の鍵で渡す（WM_CHAR の 0x09 は制御文字として捨てられる）。
void EditorWindow::press_vim_key(WPARAM word)
{
    if (word == VK_TAB)
    {
        send(application::VimKeyPress{core::VimKey{core::VimCharacter{tab_character}}});
        return;
    }
    const auto special = vim_special_for(word);
    if (special)
    {
        send(application::VimKeyPress{core::VimKey{special.value()}});
    }
}

void EditorWindow::press_plain_key(WPARAM word)
{
    const auto motion = motion_for(std::span<const KeyMotion>(plain_motions), word);
    if (motion)
    {
        send(application::MoveCaret{motion.value(), anchoring_now()});
        return;
    }
    // OS の仮想キーは開いた集合なので、既定分岐を書いてよい唯一の場所（CPP-017）。
    switch (word)
    {
    case VK_BACK:
        send(application::DeleteText{core::DeleteDirection::backward});
        return;
    case VK_DELETE:
        send(application::DeleteText{core::DeleteDirection::forward});
        return;
    case VK_RETURN:
        send(application::NewLine{});
        return;
    case VK_TAB:
        send(application::InsertText{std::string("\t")});
        return;
    case VK_ESCAPE:
        // Esc は窓を閉じない。通常モードでは選択を解くだけ（Issue #7）。
        send(application::CancelSelection{});
        return;
    default:
        break;
    }
}

void EditorWindow::press_control_key(WPARAM word)
{
    const auto motion = motion_for(std::span<const KeyMotion>(control_motions), word);
    if (motion)
    {
        send(application::MoveCaret{motion.value(), anchoring_now()});
        return;
    }
    switch (word)
    {
    case 'A':
        send(application::SelectAll{});
        return;
    case 'C':
        send(application::ClipboardAction{application::ClipboardOperation::copy});
        return;
    case 'X':
        send(application::ClipboardAction{application::ClipboardOperation::cut});
        return;
    case 'V':
        send(application::ClipboardAction{application::ClipboardOperation::paste});
        return;
    case 'Z':
        send_history(core::HistoryDirection::undo);
        return;
    case 'Y':
        send_history(core::HistoryDirection::redo);
        return;
    case 'R':
        send_vim_redo();
        return;
    case 'O':
        open_document();
        return;
    case 'S':
        if (held(VK_SHIFT))
        {
            save_document_as();
            return;
        }
        save_document();
        return;
    default:
        break;
    }
}

void EditorWindow::send_history(core::HistoryDirection direction)
{
    switch (mode_)
    {
    case core::EditMode::vim:
        // Vim では u と Ctrl-r が履歴を動かす。Ctrl+Z / Ctrl+Y はそちらに譲る。
        return;
    case core::EditMode::ordinary:
        break;
    }
    send(application::HistoryAction{direction});
}

void EditorWindow::send_vim_redo()
{
    switch (mode_)
    {
    case core::EditMode::ordinary:
        return;
    case core::EditMode::vim:
        break;
    }
    send(application::VimKeyPress{core::VimKey{core::VimSpecialKey::control_r}});
}

void EditorWindow::turn_wheel(WPARAM word)
{
    const auto delta = static_cast<std::int16_t>(HIWORD(word));
    send(application::ScrollLines{delta > 0 ? -wheel_lines : wheel_lines});
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
        return;
    }
    invalidate();
}

std::expected<void, RenderFailure> EditorWindow::draw_frame(const application::EditorFrame &frame)
{
    const auto drawn = renderer_->render(frame);
    if (drawn)
    {
        // Present が返った直後の 1 点だけが「描けた」節目（ADR 0011 の決定 1・8）。
        timing_.mark(core::Milestone::frame_presented);
    }
    return drawn;
}

void EditorWindow::present(const application::EditorFrame &frame)
{
    if (renderer_ == nullptr)
    {
        return;
    }
    const auto drawn = draw_frame(frame);
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

HWND EditorWindow::handle() const noexcept
{
    return window_;
}
} // namespace nenenib::ui::win32
