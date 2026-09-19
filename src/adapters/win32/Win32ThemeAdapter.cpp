#include "Win32ThemeAdapter.hpp"

#include "BuiltinThemes.hpp"
#include "FindHandle.hpp"
#include "LocalSettingsPath.hpp"
#include "ThemeCodec.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] core::DisplayText notice(std::string_view text)
{
    return core::DisplayText::parse(text).value();
}

[[nodiscard]] std::expected<std::vector<std::wstring>, core::DisplayText>
theme_files(const core::FilePath &directory)
{
    const auto pattern = core::to_utf16(std::string(directory.text()) + "/*.v1.theme").value();
    WIN32_FIND_DATAW data{};
    const FindHandle search(FindFirstFileW(pattern.c_str(), &data));
    std::vector<std::wstring> names;
    if (!search.valid())
    {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            return names;
        }
        return std::unexpected(notice("themes: cannot read directory"));
    }
    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }
        names.emplace_back(data.cFileName);
        if (names.size() > maximum_theme_files)
        {
            return std::unexpected(notice("themes: more than 128 files; none loaded"));
        }
    } while (FindNextFileW(search.get(), &data) != 0);
    if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        return std::unexpected(notice("themes: incomplete directory read; none loaded"));
    }
    std::sort(names.begin(), names.end());
    return names;
}

// 非正規名も診断には載せる。UTF-8 の境界と DisplayText の上限を守る。
[[nodiscard]] core::DisplayText invalid_filename(std::string_view filename)
{
    constexpr std::string_view suffix = ": invalid or reserved theme filename";
    auto length = std::min(filename.size(), core::DisplayText::maximum_bytes - suffix.size());
    while (!core::is_boundary(filename, core::Offset{length}))
    {
        --length;
    }
    const auto text =
        core::DisplayText::parse(std::string(filename.substr(0, length)) + std::string(suffix));
    return text.value_or(notice("themes: invalid theme filename"));
}

[[nodiscard]] std::expected<core::ThemeRecord, core::DisplayText>
read_record(application::FilePort &files, const core::FilePath &directory,
            const std::wstring &filename)
{
    const auto utf8 = core::to_utf8(filename);
    if (!utf8)
    {
        return std::unexpected(notice("themes: invalid filename encoding"));
    }
    const auto path = core::FilePath::parse(std::string(directory.text()) + "/" + utf8.value());
    if (!path)
    {
        return std::unexpected(invalid_filename(utf8.value()));
    }
    const auto name = theme_name_for_file(path.value());
    if (!name || name.value().text() == "system" ||
        core::theme_named(name.value().text()).has_value())
    {
        return std::unexpected(invalid_filename(utf8.value()));
    }
    auto document = load_theme(files, path.value());
    if (!document)
    {
        return core::ThemeRecord{name.value(), std::unexpected(document.error())};
    }
    return core::ThemeRecord{name.value(), core::ThemeChoice::from(std::move(document.value()))};
}

[[nodiscard]] application::ThemeInventory read_inventory(application::FilePort &files,
                                                         const core::FilePath &directory,
                                                         const std::vector<std::wstring> &names)
{
    std::vector<core::ThemeRecord> records;
    std::optional<core::DisplayText> first_notice;
    for (const auto &name : names)
    {
        auto record = read_record(files, directory, name);
        if (record)
        {
            records.push_back(std::move(record.value()));
        }
        else if (!first_notice.has_value())
        {
            first_notice = record.error();
        }
    }
    auto catalog = core::ThemeCatalog::from(std::move(records));
    if (!catalog)
    {
        return {core::ThemeCatalog::builtins(), notice("themes: conflicting names; none loaded")};
    }
    return {std::move(catalog.value()), std::move(first_notice)};
}
} // namespace

std::expected<core::FilePath, core::ThemeFailure> local_theme_directory()
{
    const auto settings = local_settings_path();
    if (!settings)
    {
        return std::unexpected(core::ThemeFailure::unreadable);
    }
    const auto path = settings.value().text();
    const auto parent = path.substr(0, path.find_last_of("/\\") + 1);
    const auto directory = core::FilePath::parse(std::string(parent) + "themes");
    if (!directory)
    {
        return std::unexpected(core::ThemeFailure::unreadable);
    }
    return directory.value();
}

Win32ThemeAdapter::Win32ThemeAdapter(application::FilePort &files,
                                     std::expected<core::FilePath, core::ThemeFailure> directory)
    : files_(files), directory_(std::move(directory))
{
}

application::ThemeInventory Win32ThemeAdapter::read()
{
    if (!directory_)
    {
        return {core::ThemeCatalog::builtins(), notice("themes: location unavailable")};
    }
    const auto names = theme_files(directory_.value());
    if (!names)
    {
        return {core::ThemeCatalog::builtins(), names.error()};
    }
    return read_inventory(files_, directory_.value(), names.value());
}
} // namespace nenenib::adapters::win32
