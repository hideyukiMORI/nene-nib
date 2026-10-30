#pragma once

#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPort.hpp"
#include "ShellRegistrationStatus.hpp"

#include <expected>

namespace nenenib::adapters::win32
{
// Windows のエクスプローラーへの登録を HKCU に書く唯一の場所（ARC-007・ADR 0178 の決定 1）。
// いまは口だけで、どれも書かずに io_error を返し、状態は absent（中身は #1087 の S4b）。
class Win32ShellRegistration final : public application::ShellRegistrationPort
{
  public:
    [[nodiscard]] std::expected<void, application::ShellRegistrationFailure>
    apply(const core::ShellRegistrationPlan &plan) override;
    [[nodiscard]] std::expected<void, application::ShellRegistrationFailure>
    remove(const core::ShellRegistrationPlan &plan) override;
    [[nodiscard]] application::ShellRegistrationStatus
    status(const core::ShellRegistrationPlan &plan) override;
};
} // namespace nenenib::adapters::win32
