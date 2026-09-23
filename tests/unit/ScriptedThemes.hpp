#pragma once

#include "DisplayText.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeInventory.hpp"
#include "ThemePort.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::tests
{
using nenenib::core::DisplayText;

class ScriptedThemes final : public nenenib::application::ThemePort
{
  public:
    explicit ScriptedThemes(
        nenenib::core::ThemeCatalog catalog = nenenib::core::ThemeCatalog::builtins(),
        std::optional<DisplayText> notice = std::nullopt)
        : catalog_(std::move(catalog)), notice_(std::move(notice))
    {
    }
    [[nodiscard]] nenenib::application::ThemeInventory read() override
    {
        ++reads_;
        return {catalog_, notice_};
    }
    [[nodiscard]] std::size_t reads() const noexcept
    {
        return reads_;
    }

  private:
    nenenib::core::ThemeCatalog catalog_;
    std::optional<DisplayText> notice_;
    std::size_t reads_ = 0;
};
} // namespace nenenib::tests
