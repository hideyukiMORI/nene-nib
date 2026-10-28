#include "FileDialog.hpp"

#include "LanguageTable.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <objbase.h>
#include <shobjidl.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
using Microsoft::WRL::ComPtr;

// 絞り込みは core の言語の表から作る（ADR 0154 の追補 2026-10-28・#849）。拡張子をここに書かない。
// 既定は 2 つ目の「すべてのファイル」（SetFileTypeIndex は 1 始まり）。
constexpr UINT all_files_index = 2;

[[nodiscard]] std::span<const COMDLG_FILTERSPEC> file_types()
{
    // ASCII の pattern は変換に失敗しない。失敗の値は空の絞り込みにせず「すべて」に倒す。
    static const std::wstring supported =
        core::to_utf16(core::file_dialog_pattern()).value_or(std::wstring{L"*.*"});
    static const std::array<COMDLG_FILTERSPEC, 2> types{
        {{L"対応するファイル", supported.c_str()}, {L"すべてのファイル", L"*.*"}}};
    return types;
}

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
    const auto types = file_types();
    static_cast<void>(dialog.SetFileTypes(static_cast<UINT>(types.size()), types.data()));
    static_cast<void>(dialog.SetFileTypeIndex(all_files_index));
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
