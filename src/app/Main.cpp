// 合成ルート（ARC-006）。ポートに実装を結び、窓を作り、メッセージループを回し、終了コードを返す。
// 端末出力の代わりは「起動できなかった理由 1 行」の MessageBoxW だけで、ログには使わない。
#include "AbsolutePath.hpp"
#include "EditorController.hpp"
#include "EditorWindow.hpp"
#include "OpenDocument.hpp"
#include "Win32AppearanceAdapter.hpp"
#include "Win32ClipboardAdapter.hpp"
#include "Win32CodePageAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <objbase.h>
#include <shellapi.h>

#include <cstddef>
#include <span>
#include <string>
#include <utility>

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

// NeNeNib.exe <path> で開く（ADR 0010 の決定 11）。最初の描画より前に意図として渡す。
void open_first_argument(nenenib::application::EditorController &controller)
{
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr || count < 2)
    {
        LocalFree(arguments);
        return;
    }
    const std::span<wchar_t *> given(arguments, static_cast<std::size_t>(count));
    const auto path = nenenib::adapters::win32::absolute_file_path(std::wstring(given[1]));
    if (path.has_value())
    {
        static_cast<void>(controller.apply(nenenib::application::OpenDocument{path.value()}));
    }
    LocalFree(arguments);
}

int run(HINSTANCE instance)
{
    nenenib::adapters::win32::Win32AppearanceAdapter appearance;
    nenenib::adapters::win32::Win32ClipboardAdapter clipboard;
    nenenib::adapters::win32::Win32FileAdapter files;
    nenenib::adapters::win32::Win32CodePageAdapter code_pages;
    nenenib::application::EditorController controller(appearance, clipboard, files, code_pages);
    open_first_argument(controller);
    const auto window = nenenib::ui::win32::EditorWindow::create(instance, controller);
    if (!window)
    {
        return report(reason_of(window.error()));
    }
    // OpenClipboard に要る HWND は窓ができてから渡す（ADR 0009 の決定 5）。
    clipboard.bind(window.value()->handle());
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
