#pragma once

#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPort.hpp"
#include "ShellRegistrationStatus.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::tests
{
// エクスプローラーへの登録の替え玉（ADR 0178 の決定 1）。レジストリに触れず、呼ばれた回数と最後の
// 計画を覚え、試験が置いた exe のパス・結果・状態を返す。apply が成功すると状態は
// registered、remove が成功すると absent になる。
class ScriptedShellRegistration final : public nenenib::application::ShellRegistrationPort
{
  public:
    using Failure = nenenib::application::ShellRegistrationFailure;
    using Status = nenenib::application::ShellRegistrationStatus;

    [[nodiscard]] std::expected<std::string, Failure> executable_path() override
    {
        ++paths_;
        if (path_failure_.has_value())
        {
            return std::unexpected(path_failure_.value());
        }
        return path_;
    }

    [[nodiscard]] std::expected<void, Failure>
    apply(const nenenib::core::ShellRegistrationPlan &plan) override
    {
        ++applies_;
        last_plan_ = plan;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        status_ = Status::registered;
        return {};
    }

    [[nodiscard]] std::expected<void, Failure>
    remove(const nenenib::core::ShellRegistrationPlan &plan) override
    {
        ++removes_;
        last_plan_ = plan;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        status_ = Status::absent;
        return {};
    }

    [[nodiscard]] Status status(const nenenib::core::ShellRegistrationPlan &plan) override
    {
        ++statuses_;
        last_plan_ = plan;
        return status_;
    }

    void fail_with(std::optional<Failure> failure) noexcept
    {
        failure_ = failure;
    }

    void set_path(std::string path)
    {
        path_ = std::move(path);
    }

    void fail_path_with(std::optional<Failure> failure) noexcept
    {
        path_failure_ = failure;
    }

    void set_status(Status status) noexcept
    {
        status_ = status;
    }

    [[nodiscard]] std::size_t paths() const noexcept
    {
        return paths_;
    }

    [[nodiscard]] std::size_t applies() const noexcept
    {
        return applies_;
    }

    [[nodiscard]] std::size_t removes() const noexcept
    {
        return removes_;
    }

    [[nodiscard]] std::size_t statuses() const noexcept
    {
        return statuses_;
    }

    // 4 つの口が呼ばれた回数の和（verify_quiet_paths）。
    [[nodiscard]] std::size_t touches() const noexcept
    {
        return paths_ + applies_ + removes_ + statuses_;
    }

    [[nodiscard]] const std::optional<nenenib::core::ShellRegistrationPlan> &
    last_plan() const noexcept
    {
        return last_plan_;
    }

  private:
    std::string path_ = "C:\\Tools\\NeNeNib\\NeNeNib.exe";
    std::optional<Failure> path_failure_;
    std::optional<Failure> failure_;
    Status status_ = Status::absent;
    std::optional<nenenib::core::ShellRegistrationPlan> last_plan_;
    std::size_t paths_ = 0;
    std::size_t applies_ = 0;
    std::size_t removes_ = 0;
    std::size_t statuses_ = 0;
};
} // namespace nenenib::tests
