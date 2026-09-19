#include "LocalSettingsPath.hpp"

#include "Utf16.hpp"

#include <windows.h>

#include <string>
#include <string_view>

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
    const bool drive = directory.size() >= 3 && directory[1] == L':' &&
                       (directory[2] == L'\\' || directory[2] == L'/');
    if (!drive && !directory.starts_with(L"\\\\"))
    {
        return std::unexpected(Failure::location_unavailable);
    }
    const auto utf8 = core::to_utf8(directory);
    if (!utf8)
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
} // namespace nenenib::adapters::win32
