#pragma once

#include "EditorSettings.hpp"
#include "SettingsIssue.hpp"
#include "ThemeCatalog.hpp"

#include <expected>
#include <optional>

namespace nenenib::application
{
class SettingsPort
{
  public:
    SettingsPort() = default;
    virtual ~SettingsPort() = default;
    SettingsPort(const SettingsPort &) = delete;
    SettingsPort(SettingsPort &&) = delete;
    SettingsPort &operator=(const SettingsPort &) = delete;
    SettingsPort &operator=(SettingsPort &&) = delete;

    [[nodiscard]] virtual std::expected<std::optional<core::EditorSettings>, SettingsIssue>
    read(const core::ThemeCatalog &themes = core::ThemeCatalog::builtins()) = 0;
    [[nodiscard]] virtual std::expected<void, SettingsIssue>
    write(const core::EditorSettings &settings) = 0;
};
} // namespace nenenib::application
