// 合成ルート（ARC-006）。ポートに実装を結び、窓を作り、メッセージループを回し、終了コードを返す。
// 端末出力の代わりは「起動できなかった理由 1 行」の MessageBoxW だけで、ログには使わない。
#include "AbsolutePath.hpp"
#include "EditorController.hpp"
#include "EditorWindow.hpp"
#include "LocalSettingsPath.hpp"
#include "Milestone.hpp"
#include "OpenDocument.hpp"
#include "Win32AppearanceAdapter.hpp"
#include "Win32BookmarkAdapter.hpp"
#include "Win32ClipboardAdapter.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32FolderAdapter.hpp"
#include "Win32HistoryAdapter.hpp"
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

int run(HINSTANCE instance)
{
    const std::vector<std::wstring> given = command_arguments();
    // 節目の受け手は常に居る。--measure が無ければ何も積まない状態のまま（決定 2）。
    nenenib::adapters::win32::Win32TimingAdapter timing;
    const std::wstring measure = option_value(given, measure_option);
    if (!measure.empty())
    {
        timing.bind(measure);
    }
    nenenib::adapters::win32::Win32AppearanceAdapter appearance;
    nenenib::adapters::win32::Win32ClipboardAdapter clipboard;
    nenenib::adapters::win32::Win32FileAdapter files;
    nenenib::adapters::win32::Win32CodePageAdapter code_pages;
    nenenib::adapters::win32::Win32SettingsAdapter settings(
        files, nenenib::adapters::win32::local_settings_path());
    nenenib::adapters::win32::Win32ThemeAdapter themes(
        files, nenenib::adapters::win32::local_theme_directory());
    // 前回のタブの一覧は窓が閉じるときに書き、ファイルの引数が無い起動で controller が読む
    // （ADR 0059 の決定 2・3・6）。
    nenenib::adapters::win32::Win32SessionAdapter session(
        files, nenenib::adapters::win32::local_session_path());
    // 閉じたファイルの履歴は閉じたときに書き、Ctrl+P の面を開くときに読む。起動の道では読まない
    // （ADR 0060 の決定 8）。
    nenenib::adapters::win32::Win32HistoryAdapter history(
        files, nenenib::adapters::win32::local_history_path());
    // 裏の仕事のワーカー 1 本と、それを借りて同じフォルダを読む adapter（ADR 0062 の決定 5・10）。
    // スレッドはここでは起きず、最初の仕事で起きる。controller より先に宣言して後に壊す
    // （壊れる順は 窓 → controller → adapter → ワーカー）。
    nenenib::adapters::win32::Win32Worker worker;
    nenenib::adapters::win32::Win32FolderAdapter folders(worker);
    nenenib::adapters::win32::Win32BookmarkAdapter bookmarks(
        files, nenenib::adapters::win32::local_bookmark_path());
    nenenib::application::EditorController controller(
        nenenib::application::EditorPorts{appearance, clipboard, files, code_pages, settings,
                                          themes, session, history, folders, bookmarks},
        initial_documents(given));
    // 起動の最初の節目。ここまでに引数の解析・adapters の構築・起動引数のファイルの読み込みと
    // 復号が済んでいる（Issue #19）。
    timing.mark(nenenib::core::Milestone::document_opened);
    const auto window = nenenib::ui::win32::EditorWindow::create(instance, controller, timing);
    if (!window)
    {
        return report(reason_of(window.error()));
    }
    // OpenClipboard に要る HWND は窓ができてから渡す（ADR 0009 の決定 5）。
    clipboard.bind(window.value()->handle());
    // 「届いた」の合図も窓ができてから渡す。番号と HWND は ui が持ち、adapters は知らない（決定
    // 7）。
    folders.bind(window.value()->work_signal());
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
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
