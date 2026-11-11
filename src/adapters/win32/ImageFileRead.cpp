#include "ImageFileRead.hpp"

#include "FileHandle.hpp"
#include "ImageLimits.hpp"
#include "NativePath.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::ImageFailure;

[[nodiscard]] Failure failure_of(DWORD error) noexcept
{
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
        error == ERROR_INVALID_NAME || error == ERROR_BAD_NETPATH)
    {
        return Failure::missing;
    }
    return Failure::unreadable;
}

// 開いた後の大きさまで確かめて読む（ディスクのファイルだけ・上限まで）。
[[nodiscard]] std::expected<std::string, Failure> read_disk_file(const FileHandle &file)
{
    if (GetFileType(file.get()) != FILE_TYPE_DISK)
    {
        return std::unexpected(Failure::unreadable);
    }
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file.get(), &size) == 0 || size.QuadPart < 0)
    {
        return std::unexpected(Failure::unreadable);
    }
    if (static_cast<std::uint64_t>(size.QuadPart) > core::image_file_byte_limit)
    {
        return std::unexpected(Failure::too_large);
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t received = 0;
    while (received < bytes.size())
    {
        DWORD read = 0;
        const auto wanted = static_cast<DWORD>(bytes.size() - received);
        if (ReadFile(file.get(), bytes.data() + received, wanted, &read, nullptr) == 0 ||
            read == 0 || read > wanted)
        {
            return std::unexpected(Failure::unreadable);
        }
        received += read;
    }
    return bytes;
}
} // namespace

std::expected<std::string, Failure> image_file_bytes(const core::FilePath &path)
{
    const auto wide = core::to_utf16(path.text());
    if (!wide.has_value() || wide.value().empty())
    {
        return std::unexpected(Failure::missing);
    }
    const std::wstring native = native_path(wide.value());
    // フォルダは CreateFileW が access_denied で断るが、理由としては「無い」（画像のファイルではな
    // い）が正しい。
    const DWORD attributes = GetFileAttributesW(native.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return std::unexpected(Failure::missing);
    }
    const FileHandle file(CreateFileW(native.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!file.valid())
    {
        return std::unexpected(failure_of(GetLastError()));
    }
    return read_disk_file(file);
}
} // namespace nenenib::adapters::win32
