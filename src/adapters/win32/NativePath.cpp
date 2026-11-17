#include "NativePath.hpp"

#include <windows.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
constexpr std::wstring_view verbatim = L"\\\\?\\";
constexpr std::wstring_view verbatim_unc = L"\\\\?\\UNC\\";
constexpr std::wstring_view device = L"\\\\.\\";
constexpr std::wstring_view unc = L"\\\\";

[[nodiscard]] constexpr bool separator(wchar_t letter) noexcept
{
    return letter == L'\\' || letter == L'/';
}

// `\\?\` か `\\.\` で始まるか（`/` で書いた `//./` も同じ）。OS は名前を読み替えない。
[[nodiscard]] bool raw_form(std::wstring_view path) noexcept
{
    return path.size() >= 4 && separator(path[0]) && separator(path[1]) &&
           (path[2] == L'?' || path[2] == L'.') && separator(path[3]);
}

// GetFullPathNameW の答えそのもの。失敗は空。
[[nodiscard]] std::wstring os_full_path(const std::wstring &path)
{
    const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (length == 0)
    {
        return {};
    }
    std::vector<wchar_t> buffer(length, L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr);
    if (written == 0 || written >= length)
    {
        return {};
    }
    return {buffer.data(), written};
}

// フォルダの部分の長さ（最後の区切りの後ろ・区切りが無ければドライブの相対 `C:` の後ろ）。
[[nodiscard]] std::size_t folder_length(std::wstring_view path) noexcept
{
    const std::size_t last = path.find_last_of(L"\\/");
    if (last != std::wstring_view::npos)
    {
        return last + 1;
    }
    return path.size() >= 2 && path[1] == L':' ? 2 : 0;
}

// OS が最後の段を装置に写した経路を、普通のファイルの経路に直す。フォルダの部分（区切りで
// 終わるか `C:` か空）は装置に写らないので OS で絶対にし、最後の段は OS と同じく末尾の空白と
// ドットを落として付け直す。最後の段が装置の名前そのもの（`nul` `NUL.` `com1 `・末尾の `:` も）
// なら OS の答え（`\\.\nul`）のまま。Windows 11 が装置に写すのはこの形だけで、拡張子つき
// （`nul.txt`）は普通のファイルにする。その振る舞いにそろえる。
[[nodiscard]] std::wstring file_path_of(const std::wstring &path, const std::wstring &mapped)
{
    const std::size_t split = folder_length(path);
    std::wstring_view name = std::wstring_view(path).substr(split);
    const std::size_t kept = name.find_last_not_of(L". ");
    const std::size_t stem = name.find_last_not_of(L". :");
    if (kept == std::wstring_view::npos || stem == std::wstring_view::npos)
    {
        return mapped;
    }
    const std::wstring_view device_name = std::wstring_view(mapped).substr(device.size());
    if (CompareStringOrdinal(name.data(), static_cast<int>(stem + 1), device_name.data(),
                             static_cast<int>(device_name.size()), TRUE) == CSTR_EQUAL)
    {
        return mapped;
    }
    const std::wstring folder =
        os_full_path(split == 0 ? std::wstring(L".") : path.substr(0, split));
    if (folder.empty())
    {
        return {};
    }
    name = name.substr(0, kept + 1);
    return folder + (separator(folder.back()) ? L"" : L"\\") + std::wstring(name);
}
} // namespace

std::wstring full_path(const std::wstring &path)
{
    const std::wstring full = os_full_path(path);
    if (full.empty() || raw_form(path) || !std::wstring_view(full).starts_with(device))
    {
        return full;
    }
    return file_path_of(path, full);
}

std::wstring native_path(const std::wstring &path)
{
    if (path.starts_with(verbatim) || path.starts_with(device))
    {
        return path;
    }
    // `\\?\` は `/`・`.`・`..` を読み替えないので、付ける前に絶対の形にする。
    const std::wstring full = full_path(path);
    if (full.empty())
    {
        return path;
    }
    // `//?/` `//./` で書いた経路は OS が `\\?\` `\\.\` に直した答えのまま。
    if (full.starts_with(verbatim) || full.starts_with(device))
    {
        return full;
    }
    if (full.starts_with(unc))
    {
        return std::wstring(verbatim_unc) + full.substr(unc.size());
    }
    return std::wstring(verbatim) + full;
}
} // namespace nenenib::adapters::win32
