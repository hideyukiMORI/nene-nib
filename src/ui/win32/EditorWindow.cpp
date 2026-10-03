#include "EditorWindow.hpp"

#include "PaletteLayout.hpp"

#include "BodyLayout.hpp"
#include "BookmarkKey.hpp"
#include "CancelComposition.hpp"
#include "CaretMotion.hpp"
#include "ClauseEmphasis.hpp"
#include "CloseTab.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DevicePixels.hpp"
#include "EditMode.hpp"
#include "EditorIntent.hpp"
#include "EndSession.hpp"
#include "FileDialog.hpp"
#include "FileFailure.hpp"
#include "FontShortcut.hpp"
#include "KeyMotion.hpp"
#include "KeyVimSpecial.hpp"
#include "Milestone.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "PointTitleBar.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "ScrollTabs.hpp"
#include "SearchHop.hpp"
#include "SelectionAnchoring.hpp"
#include "SettingsNotice.hpp"
#include "SettleRecentTab.hpp"
#include "StatusBarLayout.hpp"
#include "SwitchTab.hpp"
#include "TabCommand.hpp"
#include "TabKeyTable.hpp"
#include "TabShortcut.hpp"
#include "TabStep.hpp"
#include "TextEncoding.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "TitleBarWidth.hpp"
#include "ToggleBookmark.hpp"
#include "UnsavedTab.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSearchDirection.hpp"
#include "VimSpecialKey.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <dwmapi.h>
#include <imm.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
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
constexpr std::int32_t wheel_lines = 3;
constexpr SHORT key_down_mask = static_cast<SHORT>(0x8000);
constexpr wchar_t first_high_surrogate = 0xD800;
constexpr wchar_t first_low_surrogate = 0xDC00;
constexpr wchar_t last_low_surrogate = 0xDFFF;
constexpr wchar_t first_printable = 0x20;
constexpr wchar_t delete_character = 0x7F;
// 裏の仕事の「届いた」の合図（ADR 0062 の決定 7）。番号を持つのはここだけ。
constexpr UINT work_message = WM_APP + 1;
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
constexpr std::array<KeyVimSpecial, 11> vim_specials{{{VK_ESCAPE, core::VimSpecialKey::escape},
                                                      {VK_RETURN, core::VimSpecialKey::enter},
                                                      {VK_BACK, core::VimSpecialKey::backspace},
                                                      {VK_LEFT, core::VimSpecialKey::arrow_left},
                                                      {VK_RIGHT, core::VimSpecialKey::arrow_right},
                                                      {VK_UP, core::VimSpecialKey::arrow_up},
                                                      {VK_DOWN, core::VimSpecialKey::arrow_down},
                                                      {VK_HOME, core::VimSpecialKey::home},
                                                      {VK_END, core::VimSpecialKey::end},
                                                      {VK_PRIOR, core::VimSpecialKey::page_up},
                                                      {VK_NEXT, core::VimSpecialKey::page_down}}};
constexpr std::array<KeyVimSpecial, 4> vim_control_specials{
    {{'D', core::VimSpecialKey::control_d},
     {'U', core::VimSpecialKey::control_u},
     {'F', core::VimSpecialKey::control_f},
     {'B', core::VimSpecialKey::control_b}}};
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

[[nodiscard]] std::optional<core::VimSpecialKey> vim_control_special_for(WPARAM key) noexcept
{
    for (const KeyVimSpecial entry : vim_control_specials)
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

// Ctrl+V を矩形の鍵として送るモード（ADR 0035 の決定 9）。INSERT だけは OS の貼付に譲る。
[[nodiscard]] bool vim_block_key(core::VimMode vim) noexcept
{
    switch (vim)
    {
    case core::VimMode::insert:
        return false;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return true;
    }
    std::unreachable();
}

// Vim モードで Ctrl と一緒に押された鍵（ADR 0019 / ADR 0035 の決定 9）。表に無い鍵と、
// 矩形の入口にならない Ctrl+V は空を返し、呼ぶ側の通常モードの表へ落ちる。
[[nodiscard]] std::optional<core::VimSpecialKey> vim_control_key(WPARAM word,
                                                                 core::VimMode vim) noexcept
{
    const auto special = vim_control_special_for(word);
    if (special.has_value())
    {
        return special;
    }
    if (word == 'V' && vim_block_key(vim))
    {
        return core::VimSpecialKey::control_v;
    }
    return std::nullopt;
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
    case core::TitleBarHit::tab_close:
    case core::TitleBarHit::add_tab:
    case core::TitleBarHit::tab_list:
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
    announce_settings(opened);
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
    // 帯の幅は最初の描画の前に知らせる（アクティブなタブが見える送り量に直る・ADR 0056 の決定 8）。
    static_cast<void>(controller_.apply(application::TitleBarWidth{title_bar_width()}));
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
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_MOUSEWHEEL:
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
    case WM_ENDSESSION:
    case WM_DESTROY:
    case work_message:
        return lifetime_message(message, word, data);
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

LRESULT EditorWindow::lifetime_message(UINT message, WPARAM word, LPARAM data)
{
    // 窓の終わりの 3 通と、裏の仕事の「届いた」の合図（ADR 0062 の決定 7）。
    switch (message)
    {
    case WM_CLOSE:
        close_window();
        return 0;
    case WM_ENDSESSION:
        // OS の終了が確定したら（word が真）一覧を書く。窓は壊さない（ADR 0059 の決定 3）。
        if (word != FALSE)
        {
            remember_session(application::SessionEnd::window_closed);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case work_message:
        receive_work();
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

LRESULT EditorWindow::frame_message(UINT message, WPARAM word, LPARAM data)
{
    // 窓の大きさ・DPI・外観の 4 通。
    switch (message)
    {
    case WM_GETMINMAXINFO:
        limit_size(data);
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
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

LRESULT EditorWindow::key_message(UINT message, WPARAM word, LPARAM data)
{
    // 鍵の 3 通とフォーカスを失ったとき。Ctrl を離したとき・フォーカスを失ったときは、歩いて
    // いれば使った順を確定してから OS の既定処理へ渡す（ADR 0058 の決定 4）。
    switch (message)
    {
    case WM_KEYDOWN:
        timing_.mark(core::Milestone::input_received);
        if (!press_bookmark_key(word, data))
        {
            press_key(word);
        }
        return 0;
    case WM_CHAR:
        timing_.mark(core::Milestone::input_received);
        type_character(word);
        return 0;
    case WM_KEYUP:
        if (word == VK_CONTROL)
        {
            settle_tab_walk();
        }
        break;
    case WM_KILLFOCUS:
        settle_tab_walk();
        break;
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

// 歩いていないときは意図を送らない（Ctrl+C などのたびに描き直さない・決定 4）。
void EditorWindow::settle_tab_walk()
{
    if (controller_.tab_walking())
    {
        send(application::SettleRecentTab{});
    }
}

LRESULT EditorWindow::pointer_message(UINT message, WPARAM word, LPARAM data)
{
    // マウスの 7 通。帯の上の要素は同じ 1 本の title_bar_target で引く（ADR 0056 の決定 9）。
    switch (message)
    {
    case WM_LBUTTONDOWN:
        click_client(data);
        return 0;
    case WM_LBUTTONUP:
        release_title_bar(data);
        return 0;
    case WM_MBUTTONDOWN:
        press_middle(data);
        return 0;
    case WM_MBUTTONUP:
        middle_click(data);
        return 0;
    case WM_MOUSEMOVE:
        point_at(data);
        return 0;
    case WM_MOUSELEAVE:
        hover(std::nullopt);
        return 0;
    case WM_MOUSEWHEEL:
        turn_wheel(word, data);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window_, message, word, data);
}

void EditorWindow::limit_size(LPARAM data) const noexcept
{
    // 窓の最小の大きさは core が決める（ADR 0056 の決定 8）。窓はそれを OS へ写すだけ。
    auto *limits = std::bit_cast<MINMAXINFO *>(data);
    const auto minimum = core::minimum_window(dpi_);
    limits->ptMinTrackSize.x = core::width_of(minimum);
    limits->ptMinTrackSize.y = core::height_of(minimum);
}

core::TitleBarLayout EditorWindow::title_bar() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::title_bar_layout(controller_.title_bar_input(client.right, dpi_));
}

core::TitleBarTarget EditorWindow::title_bar_target_at(LPARAM data) const
{
    return core::title_bar_target(title_bar(), low_word_of(data), high_word_of(data));
}

bool EditorWindow::click_title_bar(LPARAM data)
{
    const auto target = title_bar_target_at(data);
    left_pressed_ = target;
    switch (target.hit)
    {
    case core::TitleBarHit::tab:
        send(application::SwitchTab{target.tab});
        return true;
    // ×・「＋」・「∨」は離したときに動かす（窓の操作と同じ）。
    case core::TitleBarHit::tab_close:
    case core::TitleBarHit::add_tab:
    case core::TitleBarHit::tab_list:
        return true;
    case core::TitleBarHit::caption:
    case core::TitleBarHit::minimize:
    case core::TitleBarHit::maximize:
    case core::TitleBarHit::close:
    case core::TitleBarHit::none:
        return false;
    }
    std::unreachable();
}

void EditorWindow::release_title_bar(LPARAM data)
{
    // 押した要素と離した要素が同じときだけ動かす。押して帯が送られ、同じ点へ来た × では閉じない。
    const auto pressed = std::exchange(left_pressed_, std::nullopt);
    if (controller_.command_palette_active())
    {
        return;
    }
    const auto target = core::title_bar_released(pressed, title_bar_target_at(data));
    if (!target.has_value())
    {
        return;
    }
    if (target.value().hit == core::TitleBarHit::tab_close)
    {
        close_tab(target.value().tab);
    }
    if (target.value().hit == core::TitleBarHit::add_tab)
    {
        send(application::NewTab{});
    }
    // 「∨」は Ctrl+P の面にタブの一覧を開く（ADR 0057 の決定 7）。面が開いている間は上で
    // 帯のクリックを受けないので、閉じるのは Esc と面の外のクリック（今の Ctrl+P と同じ）。
    if (target.value().hit == core::TitleBarHit::tab_list)
    {
        send(application::OpenTabList{});
    }
}

void EditorWindow::press_middle(LPARAM data)
{
    middle_pressed_ = std::nullopt;
    if (controller_.command_palette_active())
    {
        return;
    }
    middle_pressed_ = title_bar_target_at(data);
}

void EditorWindow::middle_click(LPARAM data)
{
    const auto pressed = std::exchange(middle_pressed_, std::nullopt);
    if (controller_.command_palette_active())
    {
        return;
    }
    const auto target = core::title_bar_released(pressed, title_bar_target_at(data));
    if (!target.has_value())
    {
        return;
    }
    if (target.value().hit == core::TitleBarHit::tab ||
        target.value().hit == core::TitleBarHit::tab_close)
    {
        close_tab(target.value().tab);
    }
}

void EditorWindow::point_at(LPARAM data)
{
    // 窓の外や caption（非クライアント領域）へ出たとき WM_MOUSELEAVE が来るように頼む。
    TRACKMOUSEEVENT track{};
    track.cbSize = static_cast<DWORD>(sizeof(track));
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = window_;
    TrackMouseEvent(&track);
    hover(core::title_bar_hover(title_bar_target_at(data)));
}

void EditorWindow::hover(const std::optional<core::TitleBarTarget> &target)
{
    // 前の値は application の状態が持つ。変わったときだけ意図を送る（ADR 0056 の決定 3）。
    RECT client{};
    GetClientRect(window_, &client);
    if (controller_.title_bar_input(client.right, dpi_).hovered == target)
    {
        return;
    }
    send(application::PointTitleBar{target});
}

void EditorWindow::close_tab(std::size_t tab)
{
    // 未保存なら映してから確かめる。取り消しか保存の失敗なら閉じない（ADR 0056 の決定 6）。
    if (application::tab_unsaved(controller_.frame().tabs, tab))
    {
        send(application::SwitchTab{tab});
        if (!confirm_discard())
        {
            return;
        }
    }
    send(application::CloseTab{tab});
}

LRESULT EditorWindow::hit_test(LPARAM data) noexcept
{
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    RECT client{};
    GetClientRect(window_, &client);
    const auto hit = core::title_bar_target(title_bar(), point.x, point.y).hit;
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
    left_pressed_ = std::nullopt;
    if (controller_.command_palette_active())
    {
        click_palette(data);
        return;
    }
    if (click_title_bar(data))
    {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const auto layout = core::status_bar_layout(client.right, client.bottom, dpi_);
    const bool in_status = core::contains(layout.band, low_word_of(data), high_word_of(data));
    if (controller_.command_line_active() && in_status)
    {
        return;
    }
    if (controller_.command_line_active())
    {
        send(application::CancelCommand{});
    }
    if (controller_.frame().command_message.has_value() && in_status)
    {
        send(application::CancelCommand{});
        return;
    }
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
    const auto body = core::body_layout(client.right, client.bottom, dpi_,
                                        controller_.frame().settings.font_size);
    if (core::contains(body.band, low_word_of(data), high_word_of(data)))
    {
        place_caret(data);
    }
}

void EditorWindow::click_palette(LPARAM data)
{
    RECT client{};
    GetClientRect(window_, &client);
    const auto frame = controller_.frame();
    if (!frame.command_palette.has_value())
    {
        return;
    }
    const auto &palette = frame.command_palette.value();
    const auto layout = core::palette_layout(client.right, client.bottom, dpi_, palette.total);
    const auto hit = core::palette_hit(layout, low_word_of(data), high_word_of(data));
    if (hit.has_value())
    {
        send(application::ActivateCommandChoice{
            core::palette_first_visible(layout, palette.selected) + hit.value()});
        return;
    }
    if (!core::contains(layout.panel, low_word_of(data), high_word_of(data)))
    {
        send(application::CancelCommand{});
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
    const auto frame = controller_.frame();
    const auto body =
        core::body_layout(client.right, client.bottom, dpi_, frame.settings.font_size);
    if (!renderer_->set_font(frame.settings))
    {
        abandon();
        return;
    }
    if (frame.lines.empty())
    {
        return;
    }
    // 行番号の欄や行より左のクリックは行頭に寄せる（最も近い位置）。
    const auto &line = frame.lines.at(row_index(body, high_word_of(data), frame.lines.size()));
    const auto column =
        renderer_->column_at(line, body, std::max(low_word_of(data), body.content.left));
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
    send(application::TitleBarWidth{title_bar_width()});
    send(application::VisibleLines{body_lines()});
}

std::int32_t EditorWindow::title_bar_width() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::to_dips(client.right, dpi_);
}

std::size_t EditorWindow::body_lines() const
{
    RECT client{};
    GetClientRect(window_, &client);
    return core::body_layout(client.right, client.bottom, dpi_,
                             controller_.frame().settings.font_size)
        .visible_lines;
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
    if (!application::composing(controller_.frame()))
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

// 構えは application の ime_stance_of が決めて frame に載せる。ui はその値を実行するだけで、
// モードや入力行の有無から IME を決めない（ADR 0061 の決定 2）。
void EditorWindow::follow_ime(const application::EditorFrame &frame)
{
    const auto previous = ime_stance_;
    ime_stance_ = frame.ime;
    switch (frame.ime)
    {
    case application::ImeStance::as_left:
        restore_ime();
        return;
    case application::ImeStance::closed:
        close_ime();
        return;
    case application::ImeStance::closed_once:
        // 入るときに 1 度だけ閉じる。続く意図では、使う人が開いた IME を閉じ直さない。
        if (previous != application::ImeStance::closed_once)
        {
            close_ime();
        }
        return;
    }
    std::unreachable();
}

void EditorWindow::close_ime()
{
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    // 控えが在れば切る前の値を上書きしない（ADR 0014 の決定 5）。閉じるのは控えの有無に依らない
    // ＝面の中で開いた IME も、Vim の NORMAL へ戻るときに閉じ直す（ADR 0061 の決定 2）。
    const bool open = ImmGetOpenStatus(context) != FALSE;
    if (ime_open_ == ImeOpenState::unrecorded)
    {
        ime_open_ = open ? ImeOpenState::open : ImeOpenState::closed;
    }
    // 閉じているときは OS に何も送らない（NORMAL の打鍵ごとに通知を起こさない）。
    if (open)
    {
        ImmSetOpenStatus(context, FALSE);
    }
    ImmReleaseContext(window_, context);
}

// 面が閉じて入力行の変換が消えたとき、IME の側に変換を残さない（ADR 0061 の決定 3）。
void EditorWindow::cancel_ime_composition()
{
    const HIMC context = ImmGetContext(window_);
    if (context == nullptr)
    {
        return;
    }
    ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
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
    // close_tab の中の send・MessageBoxW の入れ子のループの中で受けた入力の send は深さ 2 以上。
    ++sending_depth_;
    deliver(intent);
    --sending_depth_;
    // いちばん外の send の終わりで、途中で届いた合図を 1 回だけ送る。close_tab が窓を壊していたら
    // 送らない。送る send も深さを数えるので、その途中で届いた合図はまた次の 1 回にまとまる。
    if (sending_depth_ == 0 && std::exchange(work_waiting_, false) && window_ != nullptr)
    {
        send(application::WorkCompleted{});
    }
}

void EditorWindow::receive_work()
{
    if (sending_depth_ > 0)
    {
        work_waiting_ = true;
        return;
    }
    send(application::WorkCompleted{});
}

std::function<void()> EditorWindow::work_signal() const
{
    const HWND window = window_;
    // 戻り値は見ない。壊れた窓への post は失敗するだけで、合図を落としてよい（決定 7）。
    return [window] { static_cast<void>(PostMessageW(window, work_message, 0, 0)); };
}

void EditorWindow::deliver(const application::EditorIntent &intent)
{
    const bool font_change = std::holds_alternative<application::AdjustFontSize>(intent) ||
                             std::holds_alternative<application::SubmitCommand>(intent) ||
                             std::holds_alternative<application::ActivateCommandChoice>(intent);
    const float previous_size =
        font_change ? controller_.frame().settings.font_size.points() : 0.0F;
    auto frame = controller_.apply(intent);
    // 面が閉じて入力行の変換が消えたら、IME を開け閉めする前に IME の側の変換も取り消す（ADR 0061
    // の決定 3）。確定と取消は IME が自分で終えたので触らない。取り消しで IME が送り返す通知は
    // 変換の無い frame に CancelComposition を送るだけ（再入は 1 段で止まる）。
    const bool command_composed = command_composing_;
    command_composing_ = frame.command_composition.has_value();
    if (command_composed && !command_composing_ &&
        !std::holds_alternative<application::CommitText>(intent) &&
        !std::holds_alternative<application::CancelComposition>(intent))
    {
        cancel_ime_composition();
    }
    // 閉じたいタブは意図を送った結果の frame にだけ載る（ADR 0057 の決定 6）。下で frame を
    // 作り直す前に読んでおく。
    const auto close_request = frame.close_request;
    const bool closing = frame.closing;
    if (font_change)
    {
        if (std::holds_alternative<application::AdjustFontSize>(intent))
        {
            announce_settings(frame);
        }
        apply_backdrop(frame);
        if (frame.settings.font_size.points() != previous_size)
        {
            frame = controller_.apply(application::VisibleLines{body_lines()});
        }
    }
    follow_ime(frame);
    mode_ = frame.mode;
    vim_mode_ = frame.vim_mode;
    // 描くのは WM_PAINT。まとめて来た入力はここで無効化だけ積まれ、1 フレームに畳まれる（決定 6）。
    invalidate();
    update_title(frame);
    announce(frame);
    finish_tab_action(close_request, closing);
}

// frame を写し終えてから閉じる。窓を壊した後は古い frame で題名を上書きしない（ADR 0066）。
void EditorWindow::finish_tab_action(std::optional<std::size_t> close_request, bool closing)
{
    // :tabclose は × と同じ確認へ。その中の send は close_request が空なので再入は 1 段で止まる。
    if (close_request.has_value())
    {
        close_tab(close_request.value());
        return;
    }
    // Ex と通常のタブ操作が通る終了の 1 か所。確認は済んでいる。
    if (closing)
    {
        leave(application::SessionEnd::last_tab_closed);
    }
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

void EditorWindow::announce_settings(const application::EditorFrame &frame)
{
    if (frame.settings_failure.has_value())
    {
        MessageBoxW(window_, settings_notice(frame.settings_failure.value()).c_str(), product_name,
                    MB_OK | MB_ICONWARNING);
    }
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
    // 開くは今の文書を置き換えないので確かめない（ADR 0056 の決定 5）。
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
    // 未保存のタブを帯の左から順に映して確かめる（#237）。取り消しか保存の失敗でそこで止め、
    // 窓を閉じない。それまでに保存したタブは保存されたまま。
    std::size_t from = 0;
    for (auto next = application::next_unsaved_tab(controller_.frame().tabs, from);
         next.has_value(); next = application::next_unsaved_tab(controller_.frame().tabs, from))
    {
        send(application::SwitchTab{next.value()});
        if (!confirm_discard())
        {
            return;
        }
        from = next.value() + 1;
    }
    leave(application::SessionEnd::window_closed);
}

void EditorWindow::leave(application::SessionEnd reason)
{
    remember_session(reason);
    DestroyWindow(window_);
}

void EditorWindow::remember_session(application::SessionEnd reason)
{
    // send を通さない。send の後半（IME・題名・告知・無効化）は壊れていく窓に触れるので、
    // 一覧を書くだけの意図には要らない（書けなくても何も出さない・ADR 0059 の決定 3）。
    static_cast<void>(controller_.apply(application::EndSession{reason}));
}

void EditorWindow::type_character(WPARAM word)
{
    if (held(VK_CONTROL) && !held(VK_MENU))
    {
        return;
    }
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
    type_text(core::to_utf8(wide).value_or(std::string{}));
}

void EditorWindow::type_text(std::string utf8)
{
    if (controller_.command_line_active())
    {
        send(application::CommandText{std::move(utf8)});
        return;
    }
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

// Ctrl+Dの意味はcoreの表の1本。長押しの反復は登録を付け外ししない（D36・ADR 0063）。
bool EditorWindow::press_bookmark_key(WPARAM word, LPARAM data)
{
    if (word != 'D' || !held(VK_CONTROL) || held(VK_MENU))
    {
        return false;
    }
    const auto key =
        held(VK_SHIFT) ? core::BookmarkKey::control_shift_d : core::BookmarkKey::control_d;
    if (!core::toggles_bookmark(key, mode_))
    {
        return false;
    }
    constexpr LPARAM repeated_key_bit = LPARAM{1} << 30;
    if ((data & repeated_key_bit) == 0 && !application::composing(controller_.frame()))
    {
        send(application::ToggleBookmark{});
    }
    return true;
}

void EditorWindow::press_key(WPARAM word)
{
    if (controller_.command_line_active())
    {
        press_command_key(word);
        return;
    }
    if (press_tab_key(word))
    {
        return;
    }
    const auto font = font_shortcut(word);
    if (held(VK_CONTROL) && !held(VK_MENU) && font.has_value())
    {
        if (!application::composing(controller_.frame()))
        {
            send(application::AdjustFontSize{font.value(), 1});
        }
        return;
    }
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

// タブの鍵は Vim の Ctrl の表より先に引く。扱ったら true（値なしの Vim の Ctrl+W も true で、
// どの表へも流さない）。IME の変換中も効く（切り替えが変換中を捨てる・ADR 0056 の決定 4・10）。
bool EditorWindow::press_tab_key(WPARAM word)
{
    if (!held(VK_CONTROL) || held(VK_MENU))
    {
        return false;
    }
    const auto key = tab_shortcut(word, held(VK_SHIFT));
    if (!key.has_value())
    {
        return false;
    }
    const auto command = core::tab_command_for(key.value(), mode_);
    if (command.has_value())
    {
        run_tab_command(command.value());
    }
    return true;
}

void EditorWindow::run_tab_command(core::TabCommand command)
{
    switch (command)
    {
    case core::TabCommand::open:
        send(application::NewTab{});
        return;
    case core::TabCommand::next:
        send(application::WalkRecentTab{core::TabStep::next});
        return;
    case core::TabCommand::previous:
        send(application::WalkRecentTab{core::TabStep::previous});
        return;
    case core::TabCommand::close:
    {
        // 閉じるのはアクティブのタブ。位置は状態から読む（表示値を作らない・決定 9）。
        RECT client{};
        GetClientRect(window_, &client);
        close_tab(controller_.title_bar_input(client.right, dpi_).active);
        return;
    }
    }
    std::unreachable();
}

void EditorWindow::press_command_control_key(WPARAM word)
{
    switch (word)
    {
    case 'V':
        send(application::PasteCommand{});
        return;
    case 'C':
        send(application::CancelCommand{});
        return;
    case 'P':
        send(application::OpenCommandPalette{});
        return;
    // incsearch の次・前の当たり（ADR 0043 の決定 4）。検索の入力行でなければ controller が
    // 何もしない（Ex と設定一覧は今までどおり素通り）。
    case 'G':
        send(application::SearchHop{core::VimSearchDirection::forward});
        return;
    case 'T':
        send(application::SearchHop{core::VimSearchDirection::backward});
        return;
    default:
        break;
    }
}

void EditorWindow::press_command_key(WPARAM word)
{
    if (held(VK_CONTROL) && !held(VK_MENU))
    {
        press_command_control_key(word);
        return;
    }
    if (controller_.command_palette_active() && (word == VK_UP || word == VK_DOWN))
    {
        send(application::EditCommand{word == VK_UP ? core::CommandEdit::complete_previous
                                                    : core::CommandEdit::complete_next});
        return;
    }
    switch (word)
    {
    case VK_RETURN:
        send(application::SubmitCommand{});
        return;
    case VK_ESCAPE:
        send(application::CancelCommand{});
        return;
    case VK_LEFT:
        send(application::EditCommand{core::CommandEdit::left});
        return;
    case VK_RIGHT:
        send(application::EditCommand{core::CommandEdit::right});
        return;
    case VK_HOME:
        send(application::EditCommand{core::CommandEdit::home});
        return;
    case VK_END:
        send(application::EditCommand{core::CommandEdit::end});
        return;
    case VK_BACK:
        send(application::EditCommand{core::CommandEdit::backspace});
        return;
    case VK_DELETE:
        send(application::EditCommand{core::CommandEdit::erase});
        return;
    case VK_TAB:
        send(application::EditCommand{held(VK_SHIFT) ? core::CommandEdit::complete_previous
                                                     : core::CommandEdit::complete_next});
        return;
    default:
        break;
    }
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
    if (word == 'P' && !held(VK_MENU))
    {
        send(application::OpenCommandPalette{});
        return;
    }
    if (mode_ == core::EditMode::vim)
    {
        const auto special = vim_control_key(word, vim_mode_);
        if (special.has_value())
        {
            send(application::VimKeyPress{core::VimKey{special.value()}});
            return;
        }
    }
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

void EditorWindow::turn_wheel(WPARAM word, LPARAM data)
{
    const auto delta = static_cast<std::int16_t>(HIWORD(word));
    if (controller_.command_palette_active())
    {
        if (delta != 0)
        {
            send(application::EditCommand{delta > 0 ? core::CommandEdit::complete_previous
                                                    : core::CommandEdit::complete_next});
        }
        return;
    }
    if (over_title_bar(data))
    {
        scroll_tabs(delta);
        return;
    }
    if ((LOWORD(word) & MK_CONTROL) != 0)
    {
        zoom_wheel(delta);
        return;
    }
    zoom_wheel_remainder_ = 0;
    send(application::ScrollLines{delta > 0 ? -wheel_lines : wheel_lines});
}

bool EditorWindow::over_title_bar(LPARAM data) const
{
    // WM_MOUSEWHEEL の位置は画面の座標なので、クライアントの座標に直してから帯の上かを見る。
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    return core::title_bar_target(title_bar(), point.x, point.y).hit != core::TitleBarHit::none;
}

void EditorWindow::scroll_tabs(std::int32_t delta)
{
    // 刻みに満たない分は次へ繰り越す（高分解能のホイール）。奥が正・手前が負（ADR 0056 の決定 3）。
    tab_wheel_remainder_ += delta;
    const std::int32_t notches = tab_wheel_remainder_ / WHEEL_DELTA;
    tab_wheel_remainder_ %= WHEEL_DELTA;
    if (notches != 0)
    {
        send(application::ScrollTabs{notches});
    }
}

void EditorWindow::zoom_wheel(std::int32_t delta)
{
    zoom_wheel_remainder_ += delta;
    const std::int32_t steps = zoom_wheel_remainder_ / WHEEL_DELTA;
    zoom_wheel_remainder_ %= WHEEL_DELTA;
    if (steps == 0)
    {
        return;
    }
    const auto adjustment =
        steps > 0 ? core::FontSizeAdjustment::increase : core::FontSizeAdjustment::decrease;
    const auto amount = static_cast<std::size_t>(steps > 0 ? steps : -steps);
    send(application::AdjustFontSize{adjustment, amount});
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
    // 物理画素の大きさが変わらなくても DIP の幅は変わる（ADR 0056 の決定 8）。
    send(application::TitleBarWidth{title_bar_width()});
    invalidate();
}

std::expected<void, RenderFailure> EditorWindow::draw_frame(const application::EditorFrame &frame)
{
    const auto drawn = renderer_->render(frame);
    if (drawn)
    {
        // Present が返った直後の 1 点だけが「描けた」節目（ADR 0011 の決定 1・8）。
        timing_.mark(core::Milestone::frame_presented);
        if (application::composing(frame))
        {
            place_candidate_window();
        }
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
