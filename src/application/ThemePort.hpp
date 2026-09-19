#pragma once

#include "ThemeInventory.hpp"

namespace nenenib::application
{
class ThemePort
{
  public:
    ThemePort() = default;
    virtual ~ThemePort() = default;
    ThemePort(const ThemePort &) = delete;
    ThemePort(ThemePort &&) = delete;
    ThemePort &operator=(const ThemePort &) = delete;
    ThemePort &operator=(ThemePort &&) = delete;
    [[nodiscard]] virtual ThemeInventory read() = 0;
};
} // namespace nenenib::application
