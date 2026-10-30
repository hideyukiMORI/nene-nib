#pragma once

#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPort.hpp"
#include "ShellRegistrationStatus.hpp"

#include <expected>
#include <string>

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
    // 計画の木の根か共有の値が 1 つでもあるか（途中まで書かれた登録・#1183 のレビュー R1-4）。
    [[nodiscard]] bool partly_registered(const core::ShellRegistrationPlan &plan) const;

    std::wstring root_;
};
} // namespace nenenib::adapters::win32
