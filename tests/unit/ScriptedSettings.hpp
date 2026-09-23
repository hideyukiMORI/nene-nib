#pragma once

#include "EditorSettings.hpp"
#include "SettingsIssue.hpp"
#include "SettingsPort.hpp"
#include "ThemeCatalog.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace nenenib::tests
{
using SettingsReading = std::expected<std::optional<nenenib::core::EditorSettings>,
                                      nenenib::application::SettingsIssue>;

class ScriptedSettings final : public nenenib::application::SettingsPort
{
  public:
    explicit ScriptedSettings(SettingsReading reading = std::nullopt) : reading_(std::move(reading))
    {
    }

    [[nodiscard]] SettingsReading read(const nenenib::core::ThemeCatalog &) override
    {
        return reading_;
    }

    [[nodiscard]] std::expected<void, nenenib::application::SettingsIssue>
    write(const nenenib::core::EditorSettings &settings) override
    {
        ++writes_;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        written_ = settings;
        return {};
    }

    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }
    [[nodiscard]] const std::optional<nenenib::core::EditorSettings> &written() const noexcept
    {
        return written_;
    }
    void fail(std::optional<nenenib::application::SettingsIssue> failure)
    {
        failure_ = failure;
    }

  private:
    SettingsReading reading_;
    std::size_t writes_ = 0;
    std::optional<nenenib::core::EditorSettings> written_;
    std::optional<nenenib::application::SettingsIssue> failure_;
};
} // namespace nenenib::tests
