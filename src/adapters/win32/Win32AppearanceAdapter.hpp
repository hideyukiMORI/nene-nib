#pragma once

#include "Appearance.hpp"
#include "AppearancePort.hpp"
#include "AppearanceReadFailure.hpp"

#include <expected>

namespace nenenib::adapters::win32
{
// OS のテーマ設定を読む唯一の場所（ARC-003 / ARC-007）。読めない理由は型で返す。
class Win32AppearanceAdapter final : public application::AppearancePort
{
  public:
    [[nodiscard]] std::expected<core::Appearance, application::AppearanceReadFailure>
    current() const noexcept override;
};
} // namespace nenenib::adapters::win32
