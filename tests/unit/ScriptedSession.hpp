#pragma once

#include "Session.hpp"
#include "SessionFailure.hpp"
#include "SessionPort.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

namespace nenenib::tests
{
using SessionReading = std::expected<std::optional<nenenib::application::Session>,
                                     nenenib::application::SessionFailure>;

// 前回のタブの一覧の替え玉（ADR 0059 の決定 2）。仕込んだ読みを返し、書かれた値と回数を覚える。
class ScriptedSession final : public nenenib::application::SessionPort
{
  public:
    explicit ScriptedSession(SessionReading reading = std::nullopt) : reading_(std::move(reading))
    {
    }

    [[nodiscard]] SessionReading read() override
    {
        return reading_;
    }

    [[nodiscard]] std::expected<void, nenenib::application::SessionFailure>
    write(const nenenib::application::Session &session) override
    {
        ++writes_;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        written_ = session;
        return {};
    }

    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }
    [[nodiscard]] const std::optional<nenenib::application::Session> &written() const noexcept
    {
        return written_;
    }
    void fail(std::optional<nenenib::application::SessionFailure> failure)
    {
        failure_ = failure;
    }

  private:
    SessionReading reading_;
    std::size_t writes_ = 0;
    std::optional<nenenib::application::Session> written_;
    std::optional<nenenib::application::SessionFailure> failure_;
};
} // namespace nenenib::tests
