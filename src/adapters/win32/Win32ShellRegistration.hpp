#pragma once

#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPort.hpp"
#include "ShellRegistrationStatus.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <utility>

namespace nenenib::adapters::win32
{
// Windows のエクスプローラーへの登録を HKCU に書く唯一の場所（ARC-007・ADR 0178 の決定 1）。
// 計画の key_path は HKCU からの `Software\…` で、先頭の `Software` を構築子の根の副鍵に置き換えて
// 書く（製品は `Software` で恒等・試験は `Software\NeNeNib\test-<pid>`）。値はどれも REG_SZ。
class Win32ShellRegistration final : public application::ShellRegistrationPort
{
  public:
    explicit Win32ShellRegistration(std::wstring root);

    [[nodiscard]] std::expected<std::string, application::ShellRegistrationFailure>
    executable_path() override;
    [[nodiscard]] std::expected<void, application::ShellRegistrationFailure>
    apply(const core::ShellRegistrationPlan &plan) override;
    [[nodiscard]] std::expected<void, application::ShellRegistrationFailure>
    remove(const core::ShellRegistrationPlan &plan) override;
    [[nodiscard]] application::ShellRegistrationStatus
    status(const core::ShellRegistrationPlan &plan) override;

  private:
    // 計画の 5 群（木の根と共有の値）のうち今ある数と全部の数（途中の登録・#1183 / #1185
    // のレビュー）。
    [[nodiscard]] std::pair<std::size_t, std::size_t>
    present_groups(const core::ShellRegistrationPlan &plan) const;
    // 右クリックの項目の command の exe が計画の exe と同じか。
    [[nodiscard]] bool command_is_own(const core::ShellRegistrationPlan &plan) const;

    std::wstring root_;
};
} // namespace nenenib::adapters::win32
