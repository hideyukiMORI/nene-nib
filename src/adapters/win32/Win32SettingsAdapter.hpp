#pragma once

#include "FilePort.hpp"
#include "SettingsPaths.hpp"
#include "SettingsPort.hpp"
#include "SettingsVersion.hpp"

#include <expected>
#include <optional>
#include <string>

namespace nenenib::adapters::win32
{
class Win32SettingsAdapter final : public application::SettingsPort
{
  public:
    Win32SettingsAdapter(application::FilePort &files,
                         std::expected<SettingsPaths, application::SettingsFailure> paths);
    [[nodiscard]] std::expected<std::optional<core::EditorSettings>, application::SettingsIssue>
    read(const core::ThemeCatalog &themes = core::ThemeCatalog::builtins()) override;
    [[nodiscard]] std::expected<void, application::SettingsIssue>
    write(const core::EditorSettings &settings) override;

  private:
    [[nodiscard]] std::expected<std::optional<std::string>, application::SettingsFailure>
    read_bytes(const core::FilePath &path);
    [[nodiscard]] std::expected<std::optional<core::EditorSettings>, application::SettingsIssue>
    read_source(const core::ThemeCatalog &themes);
    [[nodiscard]] std::expected<void, application::SettingsFailure> check_snapshot();
    [[nodiscard]] std::expected<void, application::SettingsFailure>
    write_previous_locked(const core::EditorSettings &settings);
    [[nodiscard]] std::expected<void, application::SettingsFailure>
    write_locked(const core::EditorSettings &settings);

    application::FilePort &files_;
    std::expected<SettingsPaths, application::SettingsFailure> paths_;
    SettingsVersion source_{SettingsVersion::v2};
    std::optional<std::string> original_;
    std::optional<application::SettingsIssue> blocked_{application::SettingsFailure::not_loaded};
};
} // namespace nenenib::adapters::win32
