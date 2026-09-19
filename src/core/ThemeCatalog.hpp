#pragma once

#include "ThemeCatalogFailure.hpp"
#include "ThemeLookupFailure.hpp"
#include "ThemeRecord.hpp"

#include <span>
#include <vector>

namespace nenenib::core
{
class ThemeCatalog final
{
  public:
    [[nodiscard]] static ThemeCatalog builtins();
    [[nodiscard]] static std::expected<ThemeCatalog, ThemeCatalogFailure>
    from(std::vector<ThemeRecord> records);
    [[nodiscard]] std::expected<ThemeChoice, ThemeLookupFailure> find(const ThemeName &name) const;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::span<const ThemeRecord> records() const & noexcept;
    std::span<const ThemeRecord> records() const && = delete;

  private:
    explicit ThemeCatalog(std::shared_ptr<const std::vector<ThemeRecord>> records);
    std::shared_ptr<const std::vector<ThemeRecord>> records_;
};
} // namespace nenenib::core
