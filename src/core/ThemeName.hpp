#pragma once

#include "ThemeNameFailure.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::core
{
class ThemeName final
{
  public:
    [[nodiscard]] static std::expected<ThemeName, ThemeNameFailure> parse(std::string_view name);
    [[nodiscard]] std::string_view text() const noexcept;

  private:
    explicit ThemeName(std::string name);
    std::string name_;
};

[[nodiscard]] bool operator==(const ThemeName &left, const ThemeName &right) noexcept;
} // namespace nenenib::core
