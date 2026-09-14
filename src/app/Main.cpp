// 合成ルート（ARC-006）。ポートに実装を結び、窓を作り、メッセージループを回し、終了コードを返す。
// 端末出力の代わりは「起動できなかった理由 1 行」の MessageBoxW だけで、ログには使わない。
#include "DisplayText.hpp"
#include "EditorController.hpp"
#include "EditorWindow.hpp"
#include "TextFailure.hpp"
#include "Win32AppearanceAdapter.hpp"
#include "WindowFailure.hpp"

#include <windows.h>

#include <utility>

namespace
{
constexpr wchar_t product_name[] = L"NeNe Nib";

const wchar_t *reason_of(nenenib::core::TextFailure failure) noexcept
{
    switch (failure)
    {
    case nenenib::core::TextFailure::empty:
        return L"表示する文字列が空です。";
    case nenenib::core::TextFailure::control_character:
        return L"表示する文字列に制御文字が含まれています。";
    case nenenib::core::TextFailure::invalid_utf8:
        return L"表示する文字列が正しい UTF-8 ではありません。";
    case nenenib::core::TextFailure::too_long:
        return L"表示する文字列が長すぎます。";
    }
    std::unreachable();
}

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
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    const auto text = nenenib::core::DisplayText::parse(NENENIB_DISPLAY_TEXT);
    if (!text)
    {
        return report(reason_of(text.error()));
    }
    nenenib::adapters::win32::Win32AppearanceAdapter appearance;
    nenenib::application::EditorController controller(appearance, text.value());
    const auto window = nenenib::ui::win32::EditorWindow::create(instance, controller);
    if (!window)
    {
        return report(reason_of(window.error()));
    }
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return window.value()->rendering_failed() ? 1 : 0;
}
