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
#include "Win32TimingAdapter.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <objbase.h>
#include <shellapi.h>

#include <cstddef>
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

// 選択肢とその値を飛ばした最初の引数がファイル（ADR 0010 の決定 11）。
[[nodiscard]] std::wstring first_file(const std::vector<std::wstring> &given)
{
    std::size_t index = 0;
    while (index < given.size())
    {
        if (given[index] != measure_option)
        {
            return given[index];
        }
        index += 2;
    }
    return {};
}

// NeNeNib.exe <path> で開く（ADR 0010 の決定 11）。最初の描画より前に意図として渡す。
void open_first_file(nenenib::application::EditorController &controller,
                     const std::wstring &argument)
{
    if (argument.empty())
    {
        return;
    }
    const auto path = nenenib::adapters::win32::absolute_file_path(argument);
    if (path.has_value())
    {
        static_cast<void>(controller.apply(nenenib::application::OpenDocument{path.value()}));
    }
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
    nenenib::application::EditorController controller(appearance, clipboard, files, code_pages);
    open_first_file(controller, first_file(given));
    const auto window = nenenib::ui::win32::EditorWindow::create(instance, controller, timing);
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
