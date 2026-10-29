// 合成ルート（ARC-006）。ポートに実装を結び、窓を作り、メッセージループを回し、終了コードを返す。
// 端末出力の代わりは「起動できなかった理由 1 行」の MessageBoxW だけで、ログには使わない。
#include "AbsolutePath.hpp"
#include "BodyTextLayouts.hpp"
#include "DeviceWarmup.hpp"
#include "EditorController.hpp"
#include "EditorWindow.hpp"
#include "LocalSettingsPath.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "SystemFontFamilies.hpp"
#include "TemporaryFiles.hpp"
#include "Win32AppearanceAdapter.hpp"
#include "Win32AutosaveAdapter.hpp"
#include "Win32Bell.hpp"
#include "Win32BookmarkAdapter.hpp"
#include "Win32ClipboardAdapter.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32DocumentReader.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32FolderAdapter.hpp"
#include "Win32FontCollationAdapter.hpp"
#include "Win32HistoryAdapter.hpp"
#include "Win32LanguageServer.hpp"
#include "Win32MarkdownAdapter.hpp"
#include "Win32SessionAdapter.hpp"
#include "Win32SettingsAdapter.hpp"
#include "Win32ThemeAdapter.hpp"
#include "Win32TimingAdapter.hpp"
#include "Win32Worker.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <objbase.h>
#include <shellapi.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr wchar_t product_name[] = L"NeNe Nib";

const wchar_t *reason_of(nenenib::ui::win32::WindowFailure failure) noexcept
{
    switch (failure)
    {
    case nenenib::ui::win32::WindowFailure::class_registration:
        return L"ウィンドウクラスを登録できませんでした。";
    case nenenib::ui::win32::WindowFailure::creation:
        return L"ウィンドウを作成できませんでした。";
    case nenenib::ui::win32::WindowFailure::render:
        return L"Direct2D で描画を始められませんでした。";
    }
    std::unreachable();
}

int report(const wchar_t *reason) noexcept
{
    MessageBoxW(nullptr, reason, product_name, MB_OK | MB_ICONERROR);
    return 1;
}

// 速さの計測の出力先。付いていれば Win32TimingAdapter が記録状態で動く（ADR 0011 の決定 2）。
constexpr wchar_t measure_option[] = L"--measure";

// 引数は 1 度だけ取り出して持ち回る。CommandLineToArgvW を呼ぶ場所はここだけ（ARC-001）。
[[nodiscard]] std::vector<std::wstring> command_arguments()
{
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr || count < 1)
    {
        LocalFree(arguments);
        return {};
    }
    const std::span<wchar_t *> given(arguments, static_cast<std::size_t>(count));
    std::vector<std::wstring> result;
    for (const wchar_t *argument : given.subspan(1))
    {
        result.emplace_back(argument);
    }
    LocalFree(arguments);
    return result;
}

[[nodiscard]] std::wstring option_value(const std::vector<std::wstring> &given,
                                        std::wstring_view name)
{
    for (std::size_t index = 0; index + 1 < given.size(); ++index)
    {
        if (given[index] == name)
        {
            return given[index + 1];
        }
    }
    return {};
}

// 選択肢とその値を飛ばした引数がどれもファイル（ADR 0010 の決定 11・ADR 0056 の決定 13）。
// 絶対パスにできない引数はタブにしない。開けるかどうかは controller の OpenDocument が決める。
[[nodiscard]] std::vector<nenenib::application::OpenDocument>
initial_documents(const std::vector<std::wstring> &given)
{
    std::vector<nenenib::application::OpenDocument> documents;
    std::size_t index = 0;
    while (index < given.size())
    {
        if (given[index] == measure_option)
        {
            index += 2;
            continue;
        }
        const auto path = nenenib::adapters::win32::absolute_file_path(given[index]);
        if (path.has_value())
        {
            documents.push_back(nenenib::application::OpenDocument{path.value()});
        }
        ++index;
    }
    return documents;
}

// 届いているメッセージを全部処理する。WM_QUIT を取ったら偽。
[[nodiscard]] bool drain_messages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
        if (message.message == WM_QUIT)
        {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

// 言語サーバーの handle（出ている読み・書き・起きている子・退く子）をメッセージと一緒に待ち、
// 立った 1 つを adapter に渡す（ADR 0160 の決定 2）。handle の列は次に adapter を呼ぶまでしか
// 有効でないので、立った handle を写してから渡す。待てなければ（WAIT_FAILED）メッセージだけを
// 待つ。WM_QUIT を取ったら偽。
[[nodiscard]] bool pump_language_server(nenenib::adapters::win32::Win32LanguageServer &server)
{
    const auto handles = server.waitable_handles();
    const auto count = static_cast<DWORD>(handles.size());
    const DWORD woken = MsgWaitForMultipleObjectsEx(count, handles.data(), INFINITE, QS_ALLINPUT,
                                                    MWMO_INPUTAVAILABLE);
    if (woken < WAIT_OBJECT_0 + count)
    {
        const HANDLE raised = handles[woken - WAIT_OBJECT_0];
        server.completed(raised);
    }
    if (woken == WAIT_FAILED)
    {
        static_cast<void>(WaitMessage());
    }
    return drain_messages();
}

// 最初の frame を WARP で描いている間だけ、device の箱の event B もメッセージと一緒に待ち、
// 立てば本物の device へ差し替える（ADR 0132 の決定 4）。スレッド・タイマー・窓メッセージの
// 番号は足さない。仮でなくなれば言語サーバーの handle とメッセージを待つループに移り、二度と
// 入らない（言語サーバーの handle が無ければ今日の GetMessageW のループと同じ待ち）。
void pump_messages(nenenib::ui::win32::EditorWindow &window,
                   nenenib::ui::win32::DeviceWarmup &warmup,
                   nenenib::adapters::win32::Win32LanguageServer &server)
{
    while (window.provisional())
    {
        HANDLE event = warmup.event();
        const DWORD woken =
            MsgWaitForMultipleObjectsEx(1, &event, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        // 待てなかったとき（WAIT_FAILED）も差し替えを呼び、必ず仮を抜ける（take() が B を待つ・
        // 本窓に作れなければ終了へ）。
        if (woken != WAIT_OBJECT_0 + 1)
        {
            window.adopt_device();
        }
        if (!drain_messages())
        {
            return;
        }
    }
    while (pump_language_server(server))
    {
    }
}

// 設定の置き場を求め、前の起動が残した一時ファイルをそこから回収する（ADR 0135 の決定 3 の
// (b)）。application の port を通らないので、起動の道の port の回数（verify_quiet_paths）は
// 変わらない。
auto reclaimed_settings_paths()
{
    auto paths = nenenib::adapters::win32::local_settings_paths();
    if (paths)
    {
        nenenib::adapters::win32::reclaim_temporaries_beside(paths.value().current);
    }
    return paths;
}

// --measure があれば節目の書き先を結ぶ。
void bind_measure(nenenib::adapters::win32::Win32TimingAdapter &timing,
                  const std::vector<std::wstring> &given)
{
    const std::wstring measure = option_value(given, measure_option);
    if (!measure.empty())
    {
        timing.bind(measure);
    }
}

// OpenClipboard に要る HWND は窓ができてから渡す（ADR 0009 の決定 5）。言語サーバーの子と
// パイプ（ADR 0160 の決定 2）の「届いた」の合図も窓ができてから結ぶ（それまでは pump が回らない
// ので handle の立ちも来ない・番号は下の work_signal と同じ・設定の lsp がオフなら子は起きない）。
void bind_window_handle(const nenenib::ui::win32::EditorWindow &window,
                        nenenib::adapters::win32::Win32ClipboardAdapter &clipboard,
                        nenenib::adapters::win32::Win32LanguageServer &language_server)
{
    clipboard.bind(window.handle());
    language_server.bind(window.work_signal());
}

// 「届いた」の合図は窓ができてから渡す。番号と HWND は ui が持ち、adapters は知らない（ADR 0062
// の決定 7）。同じフォルダ・自動退避・Markdown のプレビュー（ADR 0155 の決定 5）・開く道の読み
// （ADR 0159 の決定 9）の adapter が同じワーカーを借りる。起動の引数の読みは窓より先に頼まれる
// ので、開く道の読みはそれより前に届いた結果を bind のときに 1 回知らせる。
template <typename... Adapters>
void bind_work_signal(const nenenib::ui::win32::EditorWindow &window, Adapters &...adapters)
{
    (adapters.bind(window.work_signal()), ...);
}

int run(HINSTANCE instance)
{
    const std::vector<std::wstring> given = command_arguments();
    // 節目の受け手は常に居る。--measure が無ければ何も積まない状態のまま（決定 2）。
    nenenib::adapters::win32::Win32TimingAdapter timing;
    bind_measure(timing, given);
    nenenib::adapters::win32::Win32AppearanceAdapter appearance;
    nenenib::adapters::win32::Win32ClipboardAdapter clipboard;
    nenenib::adapters::win32::Win32FileAdapter files;
    nenenib::adapters::win32::Win32CodePageAdapter code_pages;
    nenenib::adapters::win32::Win32SettingsAdapter settings(files, reclaimed_settings_paths());
    nenenib::adapters::win32::Win32ThemeAdapter themes(
        files, nenenib::adapters::win32::local_theme_directory());
    // 前回のタブの一覧は窓が閉じるときに書き、引数の無い起動で読む（ADR 0059 の決定 2・3・6）。
    nenenib::adapters::win32::Win32SessionAdapter session(
        files, nenenib::adapters::win32::local_session_paths());
    // 閉じたファイルの履歴は閉じたときに書き、Ctrl+P の面を開くときに読む。起動の道では読まない
    // （ADR 0060 の決定 8）。
    nenenib::adapters::win32::Win32HistoryAdapter history(
        files, nenenib::adapters::win32::local_history_path());
    // 裏の仕事のワーカー 1 本と、それを借りて同じフォルダを読む adapter（ADR 0062 の決定 5・10）。
    // スレッドはここでは起きず、最初の仕事で起きる。controller より先に宣言して後に壊す
    // （壊れる順は 窓 → controller → adapter → ワーカー → device の箱）。
    // device の箱は仕事に借りられるので worker より先に宣言して後に壊す。最初の仕事として
    // device を作り始め、窓を作って見せる間に重ねる（ADR 0131 の決定 2）。
    nenenib::ui::win32::DeviceWarmup warmup;
    nenenib::adapters::win32::Win32Worker worker;
    worker.post([&warmup](const std::stop_token &stop) { warmup.make(stop.stop_requested()); });
    nenenib::adapters::win32::Win32FolderAdapter folders(worker);
    // Markdown のプレビューの解析も同じワーカーを借りる（ADR 0155 の決定 5・壊れる順は folders
    // と同じ）。
    nenenib::adapters::win32::Win32MarkdownAdapter markdown(worker);
    nenenib::adapters::win32::Win32BookmarkAdapter bookmarks(
        files, nenenib::adapters::win32::local_bookmark_path());
    nenenib::ui::win32::BodyTextLayouts text_layouts;
    // 字体の一覧は ui/win32 が DirectWrite から読み、adapters が OS の照合順に並べる（#696）。
    nenenib::ui::win32::SystemFontFamilies system_fonts;
    nenenib::adapters::win32::Win32FontCollationAdapter fonts(system_fonts);
    nenenib::adapters::win32::Win32AutosaveAdapter autosave(
        worker, files, nenenib::adapters::win32::local_autosave_root());
    nenenib::adapters::win32::Win32Bell bell;
    nenenib::adapters::win32::Win32DocumentReader reads(worker, files, code_pages);
    nenenib::adapters::win32::Win32LanguageServer language_server({});
    nenenib::application::EditorController controller(
        nenenib::application::EditorPorts{
            appearance, clipboard, files, code_pages, settings, themes, session, history, folders,
            bookmarks, text_layouts, autosave, fonts, bell, markdown, reads, language_server},
        initial_documents(given));
    // 起動の最初の節目。ここまでに引数の解析・adapters の構築・起動引数のファイルの読み込みと
    // 復号が済んでいる（Issue #19）。
    timing.mark(nenenib::core::Milestone::document_opened);
    const auto window = nenenib::ui::win32::EditorWindow::create(instance, controller, timing,
                                                                 {text_layouts, warmup});
    if (!window)
    {
        return report(reason_of(window.error()));
    }
    bind_window_handle(*window.value(), clipboard, language_server);
    bind_work_signal(*window.value(), folders, autosave, markdown, reads);
    pump_messages(*window.value(), warmup, language_server);
    return window.value()->rendering_failed() ? 1 : 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // COM はファイルダイアログのために 1 度だけ（ADR 0010 の決定 10）。窓と同じ 1 本のスレッド。
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
        return report(L"COM を初期化できませんでした。");
    }
    const int code = run(instance);
    CoUninitialize();
    return code;
}
