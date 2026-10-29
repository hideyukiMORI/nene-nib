// Issue #1103: adapters の試験の置き場を GetTempPathW の下に 1 本で決める。時刻は読まない。
// 古い置き場かどうかは名前の pid と OpenProcess の判定（process_alive・ADR 0135 の決定 3）だけで
// 決める。
#include "TemporaryTestDirectory.hpp"

#include "FindHandle.hpp"
#include "NativePath.hpp"
#include "TemporaryFiles.hpp"

#include <windows.h>

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
namespace adapters = nenenib::adapters::win32;

constexpr std::wstring_view prefix = L"nib-tests-";

void remove_tree(const std::wstring &folder);

void remove_entry(const std::wstring &folder, const WIN32_FIND_DATAW &data)
{
    const std::wstring name(static_cast<const wchar_t *>(data.cFileName));
    if (name == L"." || name == L"..")
    {
        return;
    }
    const auto path = folder + L"\\" + name;
    const auto native = adapters::native_path(path);
    SetFileAttributesW(native.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
        remove_tree(path);
        return;
    }
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        RemoveDirectoryW(native.c_str());
        return;
    }
    DeleteFileW(native.c_str());
}

void remove_entries(const std::wstring &folder)
{
    WIN32_FIND_DATAW data{};
    const adapters::FindHandle search(
        FindFirstFileW(adapters::native_path(folder + L"\\*").c_str(), &data));
    if (!search.valid())
    {
        return;
    }
    do
    {
        remove_entry(folder, data);
    } while (FindNextFileW(search.get(), &data) != 0);
}

void remove_tree(const std::wstring &folder)
{
    remove_entries(folder);
    RemoveDirectoryW(adapters::native_path(folder).c_str());
}

std::optional<std::wstring> temporary_root()
{
    const DWORD length = GetTempPathW(0, nullptr);
    if (length == 0)
    {
        return std::nullopt;
    }
    std::wstring buffer(length, L'\0');
    const DWORD written = GetTempPathW(length, buffer.data());
    if (written == 0 || written >= length)
    {
        return std::nullopt;
    }
    buffer.resize(written);
    return buffer;
}

// `nib-tests-<10 進の pid>-<name>` の pid を読む。形が違えば無し（ほかの名前の置き場は触らない）。
std::optional<std::uint32_t> owner_of(std::wstring_view entry, std::wstring_view name)
{
    const auto suffix = std::wstring(L"-") + std::wstring(name);
    if (!entry.starts_with(prefix) || !entry.ends_with(suffix) ||
        entry.size() <= prefix.size() + suffix.size())
    {
        return std::nullopt;
    }
    const auto digits = entry.substr(prefix.size(), entry.size() - prefix.size() - suffix.size());
    std::string narrow;
    for (const wchar_t letter : digits)
    {
        if (letter < L'0' || letter > L'9')
        {
            return std::nullopt;
        }
        narrow.push_back(static_cast<char>(letter));
    }
    std::uint32_t pid = 0;
    const auto [end, error] = std::from_chars(narrow.data(), narrow.data() + narrow.size(), pid);
    if (error != std::errc{} || end != narrow.data() + narrow.size())
    {
        return std::nullopt;
    }
    return pid;
}

void reclaim_stale(const std::wstring &root, std::wstring_view name, std::uint32_t self)
{
    WIN32_FIND_DATAW data{};
    const auto pattern = root + std::wstring(prefix) + L"*-" + std::wstring(name);
    const adapters::FindHandle search(FindFirstFileW(pattern.c_str(), &data));
    if (!search.valid())
    {
        return;
    }
    do
    {
        const std::wstring entry(static_cast<const wchar_t *>(data.cFileName));
        const auto owner = owner_of(entry, name);
        if (owner.has_value() && (owner.value() == self || !adapters::process_alive(owner.value())))
        {
            remove_tree(root + entry);
        }
    } while (FindNextFileW(search.get(), &data) != 0);
}
} // namespace

namespace nenenib::tests
{
std::optional<std::wstring> temporary_test_directory(std::wstring_view name)
{
    const auto root = temporary_root();
    if (!root.has_value())
    {
        return std::nullopt;
    }
    const std::uint32_t self = GetCurrentProcessId();
    reclaim_stale(root.value(), name, self);
    const auto directory = root.value() + std::format(L"{}{}-{}", prefix, self, name);
    if (CreateDirectoryW(directory.c_str(), nullptr) == 0 ||
        SetCurrentDirectoryW(directory.c_str()) == 0)
    {
        return std::nullopt;
    }
    return directory;
}

void remove_temporary_test_directory(const std::wstring &directory)
{
    const auto root = temporary_root();
    if (root.has_value())
    {
        SetCurrentDirectoryW(root.value().c_str());
    }
    remove_tree(directory);
}
} // namespace nenenib::tests
