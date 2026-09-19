#include "Win32SettingsAdapter.hpp"

#include "FileHandle.hpp"
#include "SettingsCodec.hpp"
#include "Utf16.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SettingsFailure;
constexpr std::size_t maximum_settings_bytes = 4096;

[[nodiscard]] bool ensure_parent(const std::wstring &path)
{
    const auto separator = path.find_last_of(L"/\\");
    if (separator == std::wstring::npos)
    {
        return false;
    }
    const auto parent = path.substr(0, separator);
    return CreateDirectoryW(parent.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}
} // namespace

Win32SettingsAdapter::Win32SettingsAdapter(application::FilePort &files,
                                           std::expected<core::FilePath, Failure> path)
    : files_(files), path_(std::move(path))
{
}

std::expected<std::optional<std::string>, Failure> Win32SettingsAdapter::read_current()
{
    if (!path_)
    {
        return std::unexpected(path_.error());
    }
    const auto bytes = files_.read(path_.value(), maximum_settings_bytes);
    if (bytes)
    {
        return bytes.value();
    }
    if (bytes.error() == application::FileFailure::not_found)
    {
        return std::nullopt;
    }
    if (bytes.error() == application::FileFailure::too_large)
    {
        return std::unexpected(Failure::too_large);
    }
    return std::unexpected(Failure::unreadable);
}

std::expected<std::optional<core::EditorSettings>, Failure> Win32SettingsAdapter::read()
{
    const auto bytes = read_current();
    if (!bytes)
    {
        blocked_ = bytes.error();
        return std::unexpected(bytes.error());
    }
    original_ = bytes.value();
    if (!original_.has_value())
    {
        blocked_ = std::nullopt;
        return std::nullopt;
    }
    const auto decoded = decode_settings(original_.value());
    if (!decoded)
    {
        blocked_ = decoded.error();
        return std::unexpected(decoded.error());
    }
    blocked_ = std::nullopt;
    return decoded.value();
}

std::expected<void, Failure>
Win32SettingsAdapter::write_locked(const core::EditorSettings &settings)
{
    const auto current = read_current();
    if (!current)
    {
        return std::unexpected(current.error());
    }
    if (current.value() != original_)
    {
        return std::unexpected(Failure::changed_externally);
    }
    const std::string bytes = encode_settings(settings);
    const auto written = files_.write(path_.value(), bytes);
    if (!written)
    {
        return std::unexpected(Failure::unwritable);
    }
    original_ = bytes;
    return {};
}

std::expected<void, Failure> Win32SettingsAdapter::write(const core::EditorSettings &settings)
{
    if (blocked_.has_value())
    {
        return std::unexpected(blocked_.value());
    }
    const auto wide = core::to_utf16(path_.value().text());
    if (!wide || !ensure_parent(wide.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    const auto lock_path = wide.value() + L".lock";
    // 同じ保存手順の別プロセスを排他する。待たずに失敗として UI へ戻す。
    const FileHandle lock(CreateFileW(lock_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!lock.valid())
    {
        return std::unexpected(Failure::unwritable);
    }
    return write_locked(settings);
}
} // namespace nenenib::adapters::win32
