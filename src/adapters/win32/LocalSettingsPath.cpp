#include "LocalSettingsPath.hpp"

#include "AbsolutePath.hpp"
#include "Utf16.hpp"

#include <windows.h>

#include <string>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
std::expected<core::FilePath, application::SettingsFailure> local_settings_path()
{
    using Failure = application::SettingsFailure;
    constexpr wchar_t variable[] = L"LOCALAPPDATA";
    const DWORD length = GetEnvironmentVariableW(variable, nullptr, 0);
    if (length == 0 || length > 32768)
    {
        return std::unexpected(Failure::location_unavailable);
    }
    std::wstring directory(length, L'\0');
    const DWORD copied = GetEnvironmentVariableW(variable, directory.data(), length);
    if (copied == 0 || copied >= length)
    {
        return std::unexpected(Failure::location_unavailable);
    }
    directory.resize(copied);
    const auto utf8 = core::to_utf8(directory);
    if (!utf8 || !rooted_path_text(utf8.value()))
    {
        return std::unexpected(Failure::location_unavailable);
    }
    const auto path = core::FilePath::parse(utf8.value() + "/NeNeNib/settings.v1");
    if (!path)
    {
        return std::unexpected(Failure::location_unavailable);
    }
    return path.value();
}

std::optional<core::FilePath> beside_local_settings(std::string_view name)
{
    const auto settings = local_settings_path();
    if (!settings)
    {
        return std::nullopt;
    }
    const auto path = settings.value().text();
    const auto parent = path.substr(0, path.find_last_of("/\\") + 1);
    auto beside = core::FilePath::parse(std::string(parent) + std::string(name));
    if (!beside)
    {
        return std::nullopt;
    }
    return std::move(beside).value();
}
} // namespace nenenib::adapters::win32
