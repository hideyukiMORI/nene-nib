#pragma once

#include "Appearance.hpp"
#include "AppearancePort.hpp"
#include "AppearanceReadFailure.hpp"

#include <expected>
#include <utility>

namespace nenenib::tests
{
using nenenib::application::AppearancePort;
using nenenib::application::AppearanceReadFailure;
using nenenib::core::Appearance;
using Reading = std::expected<Appearance, AppearanceReadFailure>;

class ScriptedAppearance final : public AppearancePort
{
  public:
    explicit ScriptedAppearance(Reading reading) : reading_(std::move(reading)) {}

    void script(Reading reading)
    {
        reading_ = std::move(reading);
    }

    [[nodiscard]] Reading current() const noexcept override
    {
        return reading_;
    }

  private:
    Reading reading_;
};
} // namespace nenenib::tests
