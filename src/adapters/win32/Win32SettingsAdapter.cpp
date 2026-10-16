#include "Win32SettingsAdapter.hpp"

#include "FileHandle.hpp"
#include "ParentDirectory.hpp"
#include "SettingsCodec.hpp"
#include "Utf16.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::SettingsFailure;
constexpr std::size_t maximum_settings_bytes = 4096;
constexpr std::array<SettingsVersion, 3> reading_order{SettingsVersion::v3, SettingsVersion::v2,
                                                       SettingsVersion::v1};

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
        return std::string(bytes.value().view());
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
    for (const auto version : reading_order)
    {
        const auto bytes = read_bytes(settings_path(paths_.value(), version));
        if (!bytes)
        {
            return std::unexpected(bytes.error());
        }
        source_ = version;
        original_ = bytes.value();
        if (original_.has_value())
        {
            break;
        }
    }
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
    const std::optional<std::string> missing;
    for (const auto version : reading_order)
    {
        const auto bytes = read_bytes(settings_path(paths_.value(), version));
        if (!bytes)
        {
            return std::unexpected(bytes.error());
        }
        const auto &expected = version == source_ ? original_ : missing;
        if (bytes.value() != expected)
        {
            return std::unexpected(Failure::changed_externally);
        }
        if (version == source_)
        {
            return {};
        }
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
    source_ = SettingsVersion::v3;
    return {};
}

std::expected<void, Failure>
Win32SettingsAdapter::write_through(const core::EditorSettings &settings, std::size_t index)
{
    const auto version = reading_order.at(index);
    const auto name = lock_name(settings_path(paths_.value(), version));
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
    if (version == source_)
    {
        return write_locked(settings);
    }
    return write_through(settings, index + 1);
}

std::expected<void, application::SettingsIssue>
Win32SettingsAdapter::write(const core::EditorSettings &settings)
{
    if (blocked_.has_value())
    {
        return std::unexpected(blocked_.value());
    }
    // 新版から読込元まで共有無し・待機無しで取得する。旧版の不在も競合検査に含める。
    return write_through(settings, 0);
}
} // namespace nenenib::adapters::win32
