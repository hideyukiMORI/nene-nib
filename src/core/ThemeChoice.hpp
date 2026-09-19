#pragma once

#include "BuiltinTheme.hpp"
#include "ThemeDocument.hpp"

#include <memory>
#include <variant>

namespace nenenib::core
{
class ThemeChoice final
{
  public:
    [[nodiscard]] static ThemeChoice from(BuiltinTheme theme);
    [[nodiscard]] static ThemeChoice from(ThemeDocument theme);
    [[nodiscard]] Theme view() const & noexcept;
    Theme view() const && = delete;
    [[nodiscard]] std::string_view name() const & noexcept;
    std::string_view name() const && = delete;

  private:
    using Value = std::variant<BuiltinTheme, std::shared_ptr<const ThemeDocument>>;
    explicit ThemeChoice(Value value);
    Value value_;
};

[[nodiscard]] bool operator==(const ThemeChoice &left, const ThemeChoice &right) noexcept;
} // namespace nenenib::core
