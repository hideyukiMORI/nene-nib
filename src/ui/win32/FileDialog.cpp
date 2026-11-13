#include "FileDialog.hpp"

#include "LanguageTable.hpp"
#include "UiMessage.hpp"
#include "Utf16.hpp"
#include "WideUiText.hpp"

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

[[nodiscard]] std::optional<core::FilePath> shown(IFileDialog &dialog, HWND owner,
                                                  core::DisplayLanguage language)
{
    // 絞り込みの名前は表示言語から箱を出すたびに作る（ADR 0123 の追補 2026-11-13 の決定 11）。
    // COMDLG_FILTERSPEC が指す文字列は、箱を閉じるまでこの関数の局所が持つ。
    // ASCII の pattern は変換に失敗しない。失敗の値は空の絞り込みにせず「すべて」に倒す。
    const std::wstring supported =
        core::to_utf16(core::file_dialog_pattern()).value_or(std::wstring{L"*.*"});
    const auto names = file_type_names(language);
    const std::array<COMDLG_FILTERSPEC, 2> types{
        {{names.at(0).c_str(), supported.c_str()}, {names.at(1).c_str(), L"*.*"}}};
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

// 場所を箱の左の一覧の下に足す（ADR 0162 の追補 #1450）。SHCreateItemFromParsingName は経路を
// 解くだけで中身を列挙しない（WSL の VM が止まっていても起こさない・実測）。失敗は黙って外す。
void add_places(IFileDialog &dialog, std::span<const std::string> places)
{
    for (const auto &place : places)
    {
        const auto wide = core::to_utf16(place);
        if (!wide.has_value())
        {
            continue;
        }
        ComPtr<IShellItem> item;
        if (SUCCEEDED(SHCreateItemFromParsingName(wide.value().c_str(), nullptr,
                                                  IID_PPV_ARGS(item.GetAddressOf()))))
        {
            static_cast<void>(dialog.AddPlace(item.Get(), FDAP_BOTTOM));
        }
    }
}
} // namespace

std::array<std::wstring, 2> file_type_names(core::DisplayLanguage language)
{
    return {wide_ui_text(core::UiMessage::os_filter_supported_files, language),
            wide_ui_text(core::UiMessage::os_filter_all_files, language)};
}

std::optional<core::FilePath> choose_file_to_open(HWND owner, core::DisplayLanguage language)
{
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(dialog.GetAddressOf()))))
    {
        return std::nullopt;
    }
    return shown(*dialog.Get(), owner, language);
}

std::optional<core::FilePath> choose_file_to_save(HWND owner, const std::wstring &suggested,
                                                  core::DisplayLanguage language)
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
    return shown(*dialog.Get(), owner, language);
}

std::optional<core::FilePath> choose_folder(HWND owner, std::span<const std::string> places)
{
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(dialog.GetAddressOf()))))
    {
        return std::nullopt;
    }
    // フォルダだけを選ぶ。ファイルの種類の絞り込みは置かない（フォルダの窓では使わない）。
    FILEOPENDIALOGOPTIONS options = 0;
    if (FAILED(dialog->GetOptions(&options)) ||
        FAILED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM)))
    {
        return std::nullopt;
    }
    add_places(*dialog.Get(), places);
    if (FAILED(dialog->Show(owner)))
    {
        return std::nullopt;
    }
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(item.GetAddressOf())) || item.Get() == nullptr)
    {
        return std::nullopt;
    }
    return path_of(*item.Get());
}
} // namespace nenenib::ui::win32
