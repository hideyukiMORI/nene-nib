#pragma once

#include "Session.hpp"
#include "SessionFailure.hpp"
#include "SessionPort.hpp"

#include <expected>
#include <optional>

namespace nenenib::application
{
// 何も覚えない一覧の口（Issue #252 の工程 1）。read は「一覧が無い」、write は何もしない成功。
// 合成ルートが本物の adapter（ADR 0059 の決定 2）を結ぶまでの間だけ渡す。
class NoSession final : public SessionPort
{
  public:
    [[nodiscard]] std::expected<std::optional<Session>, SessionFailure> read() override
    {
        return std::optional<Session>{};
    }
    [[nodiscard]] std::expected<void, SessionFailure> write(const Session &) override
    {
        return {};
    }
};
} // namespace nenenib::application
