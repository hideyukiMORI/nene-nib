#pragma once

#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationStatus.hpp"

#include <expected>
#include <string>

namespace nenenib::application
{
// Windows のエクスプローラーへの登録の唯一の入口（ADR 0178 の決定 1）。レジストリに触れるのは実装
// （src/adapters/win32）だけで（ARC-007）、controller が呼ぶのは登録の 2 つの操作の意図のときだけ
// （決定 3・4）。鍵と値は core の shell_registration_plan が作り、実装はそれを写すだけ。
class ShellRegistrationPort
{
  public:
    ShellRegistrationPort() = default;
    virtual ~ShellRegistrationPort() = default;
    ShellRegistrationPort(const ShellRegistrationPort &) = delete;
    ShellRegistrationPort(ShellRegistrationPort &&) = delete;
    ShellRegistrationPort &operator=(const ShellRegistrationPort &) = delete;
    ShellRegistrationPort &operator=(ShellRegistrationPort &&) = delete;

    // 自分の exe の完全なパス（UTF-8）。計画の材料で、登録の操作のときだけ呼ぶ（決定 1・4）。
    [[nodiscard]] virtual std::expected<std::string, ShellRegistrationFailure>
    executable_path() = 0;
    // 計画の鍵と値を書く（既にある値は上書き）。
    [[nodiscard]] virtual std::expected<void, ShellRegistrationFailure>
    apply(const core::ShellRegistrationPlan &plan) = 0;
    // 計画の鍵と値を消す。
    [[nodiscard]] virtual std::expected<void, ShellRegistrationFailure>
    remove(const core::ShellRegistrationPlan &plan) = 0;
    // 計画の登録が今あるか。
    [[nodiscard]] virtual ShellRegistrationStatus
    status(const core::ShellRegistrationPlan &plan) = 0;
};
} // namespace nenenib::application
