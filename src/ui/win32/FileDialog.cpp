#include "FileDialog.hpp"

#include "Utf16.hpp"

#include <windows.h>

#include <objbase.h>
#include <shobjidl.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
using Microsoft::WRL::ComPtr;

constexpr std::array<COMDLG_FILTERSPEC, 2> file_types{
    {{L"テキスト ファイル", L"*.txt;*.md;*.markdown"}, {L"すべてのファイル", L"*.*"}}};

// COM が返した UTF-16 の経路を検証済みの core::FilePath へ。解放はここで閉じる（CPP-016）。
// 変換は core::to_utf8 ただ 1 本で、変換できない経路は空になり parse が拒む（Issue #13）。
[[nodiscard]] std::optional<core::FilePath> path_of(IShellItem &item)
{
    PWSTR wide = nullptr;
    if (FAILED(item.GetDisplayName(SIGDN_FILESYSPATH, &wide)) || wide == nullptr)
    {
        return std::nullopt;
    }
    auto parsed =
        core::FilePath::parse(core::to_utf8(std::wstring_view(wide)).value_or(std::string{}));
    CoTaskMemFree(wide);
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

[[nodiscard]] std::optional<core::FilePath> shown(IFileDialog &dialog, HWND owner)
{
    static_cast<void>(dialog.SetFileTypes(static_cast<UINT>(file_types.size()), file_types.data()));
    // 取り消しは HRESULT の失敗として返る。理由を分けずに「選ばれなかった」として扱う。
    if (FAILED(dialog.Show(owner)))
    {
        return std::nullopt;
    }
    ComPtr<IShellItem> item;
    if (FAILED(dialog.GetResult(item.GetAddressOf())) || item.Get() == nullptr)
    {
        return std::nullopt;
    }
    return path_of(*item.Get());
}
} // namespace

std::optional<core::FilePath> choose_file_to_open(HWND owner)
{
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(dialog.GetAddressOf()))))
    {
        return std::nullopt;
    }
    return shown(*dialog.Get(), owner);
}

std::optional<core::FilePath> choose_file_to_save(HWND owner, const std::wstring &suggested)
{
    ComPtr<IFileSaveDialog> dialog;
    if (FAILED(CoCreateInstance(__uuidof(FileSaveDialog), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(dialog.GetAddressOf()))))
    {
        return std::nullopt;
    }
    static_cast<void>(dialog->SetFileName(suggested.c_str()));
    // 拡張子を打たずに決めた名前は .txt になる（IFileDialog の既定の振る舞い）。
    static_cast<void>(dialog->SetDefaultExtension(L"txt"));
    return shown(*dialog.Get(), owner);
}
} // namespace nenenib::ui::win32
