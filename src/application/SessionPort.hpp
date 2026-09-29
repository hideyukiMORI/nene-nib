#pragma once

#include "Session.hpp"
#include "SessionFailure.hpp"

#include <expected>
#include <optional>

namespace nenenib::application
{
// 前回のタブの一覧を読み書きする口（ADR 0059 の決定 2）。保存形式と場所は adapters に閉じる。
// read の値なしは「一覧が無い」（初めての起動）。
class SessionPort
{
  public:
    SessionPort() = default;
    virtual ~SessionPort() = default;
    SessionPort(const SessionPort &) = delete;
    SessionPort(SessionPort &&) = delete;
    SessionPort &operator=(const SessionPort &) = delete;
    SessionPort &operator=(SessionPort &&) = delete;

    [[nodiscard]] virtual std::expected<std::optional<Session>, SessionFailure> read() = 0;
    [[nodiscard]] virtual std::expected<void, SessionFailure> write(const Session &session) = 0;
};
} // namespace nenenib::application
