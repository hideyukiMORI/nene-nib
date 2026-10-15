#include "TemporaryFiles.hpp"

#include "FindHandle.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <array>
#include <charconv>
#include <string_view>

namespace nenenib::adapters::win32
{
namespace
{
constexpr std::wstring_view temporary_marker = L".nib-tmp-";

[[nodiscard]] std::wstring digits(std::uint32_t value, int base)
{
    std::array<char, 16> text{};
    const auto end = std::to_chars(text.data(), text.data() + text.size(), value, base).ptr;
    return std::wstring(text.data(), end);
}

[[nodiscard]] std::wstring folder_of(const std::wstring &path)
{
    return path.substr(0, path.find_last_of(L"/\\") + 1);
}

// 短い名前（8.3）でパターンに当たったものを消さないよう、返った長い名前を確かめる。
[[nodiscard]] bool starts_with_ignoring_case(std::wstring_view name, std::wstring_view prefix)
{
    // 長さ 0 の比較は CompareStringOrdinal が失敗で返すので、空の接頭辞はここで通す。
    return prefix.empty() ||
           (name.size() >= prefix.size() &&
            CompareStringOrdinal(name.data(), static_cast<int>(prefix.size()), prefix.data(),
                                 static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL);
}

void reclaim_matching(const std::wstring &folder, std::wstring_view prefix,
                      const std::wstring &pattern)
{
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data,
                                             FindExSearchNameMatch, nullptr, 0));
    if (!search.valid())
    {
        return;
    }
    do
    {
        const std::wstring_view name(static_cast<const wchar_t *>(data.cFileName));
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            starts_with_ignoring_case(name, prefix) &&
            name.find(temporary_marker) != std::wstring_view::npos)
        {
            static_cast<void>(DeleteFileW((folder + std::wstring(name)).c_str()));
        }
    } while (FindNextFileW(search.get(), &data) != 0);
}
} // namespace

std::wstring temporary_name(const std::wstring &target, std::uint32_t sequence)
{
    return target + std::wstring(temporary_marker) + digits(GetCurrentProcessId(), 16) + L'-' +
           digits(sequence, 10);
}

bool owned_temporary(const std::wstring &temporary, const std::wstring &target,
                     std::uint32_t sequence)
{
    return temporary == temporary_name(target, sequence);
}

void reclaim_temporaries_of(const std::wstring &target)
{
    const std::wstring folder = folder_of(target);
    const std::wstring prefix = target.substr(folder.size()) + std::wstring(temporary_marker);
    reclaim_matching(folder, prefix, target + std::wstring(temporary_marker) + L'*');
}

void reclaim_temporaries_beside(const core::FilePath &file)
{
    const auto wide = core::to_utf16(file.text());
    if (!wide)
    {
        return;
    }
    const std::wstring folder = folder_of(wide.value());
    reclaim_matching(folder, {}, folder + L'*' + std::wstring(temporary_marker) + L'*');
}
} // namespace nenenib::adapters::win32
