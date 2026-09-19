#pragma once

#include "FilePort.hpp"
#include "ThemePort.hpp"

#include <cstddef>
#include <expected>

namespace nenenib::adapters::win32
{
inline constexpr std::size_t maximum_theme_files = 128;

[[nodiscard]] std::expected<core::FilePath, core::ThemeFailure> local_theme_directory();

class Win32ThemeAdapter final : public application::ThemePort
{
  public:
    Win32ThemeAdapter(application::FilePort &files,
                      std::expected<core::FilePath, core::ThemeFailure> directory);
    [[nodiscard]] application::ThemeInventory read() override;

  private:
    application::FilePort &files_;
    std::expected<core::FilePath, core::ThemeFailure> directory_;
};
} // namespace nenenib::adapters::win32
