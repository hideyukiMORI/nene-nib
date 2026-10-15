#include "Win32FileAdapter.hpp"

#include "AbsolutePath.hpp"
#include "CompleteFileRead.hpp"
#include "FileHandle.hpp"
#include "TemporaryFiles.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::FileFailure;
using Mode = application::FileWriteMode;

// WriteFile が 1 回に受け取れるのは DWORD の分まで。上限ではなく分割で書く（決定 6）。
constexpr std::size_t write_chunk_bytes = 32U * 1024U * 1024U;
// 同じ名前が既にあれば（プロセス ID の再利用）連番を進めて試す回数（ADR 0135 の決定 2）。
constexpr std::uint32_t temporary_attempts = 8;

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

[[nodiscard]] std::expected<core::SealedBytes, Failure> read_all(const FileHandle &file,
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
    const auto requested = static_cast<std::size_t>(size.QuadPart);
    return complete_file_read(
        requested,
        [&file](std::span<char> target) noexcept -> std::expected<std::size_t, Failure>
        {
            DWORD done = 0;
            if (ReadFile(file.get(), target.data(), static_cast<DWORD>(target.size()), &done,
                         nullptr) == 0)
            {
                return std::unexpected(Failure::unreadable);
            }
            return static_cast<std::size_t>(done);
        });
}

[[nodiscard]] std::expected<void, Failure> write_all(const FileHandle &file, std::string_view bytes)
{
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
    return {};
}

[[nodiscard]] std::expected<void, Failure> write_parts(const FileHandle &file,
                                                       std::span<const std::string_view> parts)
{
    for (const std::string_view part : parts)
    {
        const auto written = write_all(file, part);
        if (!written)
        {
            return written;
        }
    }
    // 全断片の後に1回だけ届かせる。置換前なので失敗しても元は残る（ADR 0119）。
    if (FlushFileBuffers(file.get()) == 0)
    {
        return std::unexpected(Failure::unwritable);
    }
    return {};
}

// 作れなかった一時ファイルは自分のものではないので消さない（ADR 0067）。名前が既にあれば
// false を返し、呼び出し側が次の連番で試す（ADR 0135 の決定 2）。
[[nodiscard]] std::expected<bool, Failure> refused_creation(DWORD error) noexcept
{
    if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
    {
        return false;
    }
    return std::unexpected(failure_of(error, Failure::unwritable));
}

// どのモードでも CREATE_NEW・共有 0 で作り、全断片を書いて閉じる（ADR 0135 の決定 2）。
// 書けたら true、名前が既にあれば false。
[[nodiscard]] std::expected<bool, Failure>
written_temporary(const std::wstring &temporary, std::span<const std::string_view> parts)
{
    std::expected<void, Failure> written;
    {
        const FileHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file.valid())
        {
            return refused_creation(GetLastError());
        }
        written = write_parts(file, parts);
    }
    if (!written)
    {
        DeleteFileW(temporary.c_str());
        return std::unexpected(written.error());
    }
    return true;
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
[[nodiscard]] bool placed(const std::wstring &target, const std::wstring &temporary,
                          Mode mode) noexcept
{
    switch (mode)
    {
    case Mode::replace:
        return replaced(target, temporary);
    case Mode::create_new:
        return MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
    }
    std::unreachable();
}

[[nodiscard]] Failure placement_failure(DWORD error, Mode mode) noexcept
{
    if (mode == Mode::create_new && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS))
    {
        return Failure::already_exists;
    }
    return Failure::unwritable;
}

// 置き換えの直前に、一時ファイルがこのプロセスのこの連番のものかを名前で確かめる（決定 4）。
[[nodiscard]] std::expected<void, Failure> placed_temporary(const std::wstring &target,
                                                            const std::wstring &temporary,
                                                            std::uint32_t sequence, Mode mode)
{
    if (!owned_temporary(temporary, target, sequence))
    {
        return std::unexpected(Failure::unwritable);
    }
    if (!placed(target, temporary, mode))
    {
        const auto failure = placement_failure(GetLastError(), mode);
        DeleteFileW(temporary.c_str());
        return std::unexpected(failure);
    }
    return {};
}
} // namespace

std::expected<core::SealedBytes, Failure> Win32FileAdapter::read(const core::FilePath &path,
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
                                                     std::span<const std::string_view> parts,
                                                     Mode mode)
{
    const std::wstring target = widen(path.text());
    if (target.empty())
    {
        return std::unexpected(Failure::unwritable);
    }
    if (mode == Mode::create_new && GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        return std::unexpected(Failure::already_exists);
    }
    // 前の保存が残した一時ファイルを、この保存先の保存の直前に回収する（ADR 0135 の決定 3）。
    reclaim_temporaries_of(target);
    for (std::uint32_t attempt = 0; attempt < temporary_attempts; ++attempt)
    {
        ++sequence_;
        const std::wstring temporary = temporary_name(target, sequence_);
        const auto written = written_temporary(temporary, parts);
        if (!written)
        {
            return std::unexpected(written.error());
        }
        if (written.value())
        {
            return placed_temporary(target, temporary, sequence_, mode);
        }
    }
    return std::unexpected(Failure::unwritable);
}

std::expected<core::FilePath, Failure> Win32FileAdapter::resolve(const core::FilePath &path)
{
    auto absolute = absolute_file_path(widen(path.text()));
    if (!absolute.has_value())
    {
        return std::unexpected(Failure::unwritable);
    }
    return std::move(absolute).value();
}

bool Win32FileAdapter::same_file(const core::FilePath &left, const core::FilePath &right) const
{
    const std::wstring first = widen(left.text());
    const std::wstring second = widen(right.text());
    if (first.empty() || second.empty())
    {
        return false;
    }
    return CompareStringOrdinal(first.c_str(), static_cast<int>(first.size()), second.c_str(),
                                static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}
} // namespace nenenib::adapters::win32
