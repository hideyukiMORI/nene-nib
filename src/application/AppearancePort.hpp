#pragma once

#include "Appearance.hpp"
#include "AppearanceReadFailure.hpp"

#include <expected>

namespace nenenib::application
{
// OS の外観設定を読む唯一の入口。実装は src/adapters/win32 だけが持つ（ARC-003 / ARC-007）。
class AppearancePort
{
  public:
    AppearancePort() = default;
    virtual ~AppearancePort() = default;
    AppearancePort(const AppearancePort &) = delete;
    AppearancePort(AppearancePort &&) = delete;
    AppearancePort &operator=(const AppearancePort &) = delete;
    AppearancePort &operator=(AppearancePort &&) = delete;

    [[nodiscard]] virtual std::expected<core::Appearance, AppearanceReadFailure>
    current() const noexcept = 0;
};
} // namespace nenenib::application
