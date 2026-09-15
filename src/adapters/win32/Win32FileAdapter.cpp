#include "Win32FileAdapter.hpp"

#include "FileHandle.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cstddef>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::FileFailure;

// WriteFile が 1 回に受け取れるのは DWORD の分まで。上限ではなく分割で書く（決定 6）。
constexpr std::size_t write_chunk_bytes = 32U * 1024U * 1024U;
constexpr wchar_t temporary_suffix[] = L".nib-tmp";

// UTF-8 の経路を Win32 の UTF-16 へ。変換そのものは core::to_utf16 ただ 1 本（CPP-014）。
// 変換できない経路は空になり、呼び出し側が not_found / unwritable で断る（Issue #13）。
[[nodiscard]] std::wstring widen(std::string_view utf8)
{
    return core::to_utf16(utf8).value_or(std::wstring{});
}

[[nodiscard]] Failure failure_of(DWORD error, Failure fallback) noexcept
{
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
        error == ERROR_INVALID_NAME)
    {
        return Failure::not_found;
    }
    if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION)
    {
        return Failure::access_denied;
    }
    return fallback;
}

[[nodiscard]] bool directory(const std::wstring &path) noexcept
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

[[nodiscard]] std::expected<std::string, Failure> read_all(const FileHandle &file,
                                                           std::size_t maximum_bytes)
{
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file.get(), &size) == 0)
    {
        return std::unexpected(Failure::unreadable);
    }
    // 大きさを見てから読む。上限を超えるファイルは 1 バイトも抱えない（決定 12）。
    if (size.QuadPart < 0 || static_cast<unsigned long long>(size.QuadPart) > maximum_bytes)
    {
        return std::unexpected(Failure::too_large);
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD done = 0;
    if (!bytes.empty() &&
        ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &done, nullptr) == 0)
    {
        return std::unexpected(Failure::unreadable);
    }
    bytes.resize(done);
    return bytes;
}

[[nodiscard]] std::expected<void, Failure> write_all(const std::wstring &path,
                                                     std::string_view bytes)
{
    const FileHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid())
    {
        return std::unexpected(failure_of(GetLastError(), Failure::unwritable));
    }
    std::size_t written = 0;
    while (written < bytes.size())
    {
        const std::string_view part = bytes.substr(written, write_chunk_bytes);
        DWORD done = 0;
        if (WriteFile(file.get(), part.data(), static_cast<DWORD>(part.size()), &done, nullptr) ==
                0 ||
            done != part.size())
        {
            return std::unexpected(Failure::unwritable);
        }
        written += done;
    }
    // 置き換える前にディスクへ届かせる。ここで落ちても元のファイルはまだ触っていない（決定 6）。
    if (FlushFileBuffers(file.get()) == 0)
    {
        return std::unexpected(Failure::unwritable);
    }
    return {};
}

// 既存なら ReplaceFileW が属性と ACL を保ったまま入れ替え、無ければ MoveFileExW で置く（決定 6）。
[[nodiscard]] bool replaced(const std::wstring &target, const std::wstring &temporary) noexcept
{
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        return ReplaceFileW(target.c_str(), temporary.c_str(), nullptr,
                            REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != 0;
    }
    return MoveFileExW(temporary.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
} // namespace

std::expected<std::string, Failure> Win32FileAdapter::read(const core::FilePath &path,
                                                           std::size_t maximum_bytes)
{
    const std::wstring wide = widen(path.text());
    if (wide.empty())
    {
        return std::unexpected(Failure::not_found);
    }
    // フォルダは CreateFileW が access_denied で断るが、理由としては「読めない」が正しい。
    if (directory(wide))
    {
        return std::unexpected(Failure::unreadable);
    }
    const FileHandle file(CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid())
    {
        return std::unexpected(failure_of(GetLastError(), Failure::unreadable));
    }
    return read_all(file, maximum_bytes);
}

std::expected<void, Failure> Win32FileAdapter::write(const core::FilePath &path,
                                                     std::string_view bytes)
{
    const std::wstring target = widen(path.text());
    if (target.empty())
    {
        return std::unexpected(Failure::unwritable);
    }
    const std::wstring temporary = target + temporary_suffix;
    const auto written = write_all(temporary, bytes);
    if (!written)
    {
        DeleteFileW(temporary.c_str());
        return written;
    }
    if (!replaced(target, temporary))
    {
        DeleteFileW(temporary.c_str());
        return std::unexpected(Failure::unwritable);
    }
    return {};
}
} // namespace nenenib::adapters::win32
