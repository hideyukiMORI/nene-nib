#include "Win32SettingsAdapter.hpp"

#include "FileHandle.hpp"
#include "ParentDirectory.hpp"
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

[[nodiscard]] std::expected<std::wstring, Failure> lock_name(const core::FilePath &path)
{
    const auto wide = core::to_utf16(path.text());
    if (!wide || !ensure_parent_directory(wide.value()))
    {
        return std::unexpected(Failure::unwritable);
    }
    return wide.value() + L".lock";
}
} // namespace

Win32SettingsAdapter::Win32SettingsAdapter(application::FilePort &files,
                                           std::expected<SettingsPaths, Failure> paths)
    : files_(files), paths_(std::move(paths))
{
}

std::expected<std::optional<std::string>, Failure>
Win32SettingsAdapter::read_bytes(const core::FilePath &path)
{
    const auto bytes = files_.read(path, maximum_settings_bytes);
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

std::expected<std::optional<core::EditorSettings>, application::SettingsIssue>
Win32SettingsAdapter::read_source(const core::ThemeCatalog &themes)
{
    if (!paths_)
    {
        return std::unexpected(paths_.error());
    }
    auto bytes = read_bytes(paths_.value().current);
    source_ = SettingsVersion::v2;
    if (bytes && !bytes.value().has_value())
    {
        bytes = read_bytes(paths_.value().previous);
        source_ = SettingsVersion::v1;
    }
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    original_ = bytes.value();
    if (!original_.has_value())
    {
        return std::nullopt;
    }
    const auto decoded = decode_settings(original_.value(), source_, themes);
    if (!decoded)
    {
        return std::unexpected(decoded.error());
    }
    return decoded.value();
}

std::expected<std::optional<core::EditorSettings>, application::SettingsIssue>
Win32SettingsAdapter::read(const core::ThemeCatalog &themes)
{
    const auto loaded = read_source(themes);
    blocked_ = std::nullopt;
    if (!loaded)
    {
        blocked_ = loaded.error();
    }
    return loaded;
}

std::expected<void, Failure> Win32SettingsAdapter::check_snapshot()
{
    const auto current = read_bytes(paths_.value().current);
    if (!current)
    {
        return std::unexpected(current.error());
    }
    switch (source_)
    {
    case SettingsVersion::v2:
        if (current.value() != original_)
        {
            return std::unexpected(Failure::changed_externally);
        }
        return {};
    case SettingsVersion::v1:
        if (current.value().has_value())
        {
            return std::unexpected(Failure::changed_externally);
        }
        const auto previous = read_bytes(paths_.value().previous);
        if (!previous)
        {
            return std::unexpected(previous.error());
        }
        if (previous.value() != original_)
        {
            return std::unexpected(Failure::changed_externally);
        }
        return {};
    }
    std::unreachable();
}

std::expected<void, Failure>
Win32SettingsAdapter::write_locked(const core::EditorSettings &settings)
{
    const auto checked = check_snapshot();
    if (!checked)
    {
        return std::unexpected(checked.error());
    }
    const std::string bytes = encode_settings(settings);
    const auto written = files_.write(paths_.value().current, bytes);
    if (!written)
    {
        return std::unexpected(Failure::unwritable);
    }
    original_ = bytes;
    source_ = SettingsVersion::v2;
    return {};
}

std::expected<void, Failure>
Win32SettingsAdapter::write_previous_locked(const core::EditorSettings &settings)
{
    const auto name = lock_name(paths_.value().previous);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    const FileHandle lock(CreateFileW(name.value().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!lock.valid())
    {
        return std::unexpected(Failure::unwritable);
    }
    return write_locked(settings);
}

std::expected<void, application::SettingsIssue>
Win32SettingsAdapter::write(const core::EditorSettings &settings)
{
    if (blocked_.has_value())
    {
        return std::unexpected(blocked_.value());
    }
    const auto name = lock_name(paths_.value().current);
    if (!name)
    {
        return std::unexpected(name.error());
    }
    // 新版、移行元の順で、共有無し・待機無しのlockを取得する。
    const FileHandle lock(CreateFileW(name.value().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!lock.valid())
    {
        return std::unexpected(Failure::unwritable);
    }
    switch (source_)
    {
    case SettingsVersion::v1:
        return write_previous_locked(settings);
    case SettingsVersion::v2:
        return write_locked(settings);
    }
    std::unreachable();
}
} // namespace nenenib::adapters::win32
