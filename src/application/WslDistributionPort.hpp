#pragma once

#include "WslDistribution.hpp"
#include "WslListFailure.hpp"

#include <expected>
#include <vector>

namespace nenenib::application
{
// 登録された WSL のディストリを読む唯一の入口（ADR 0162 の追補 #1450）。実装は src/adapters/win32
// だけが持ち（ARC-007）、process を起こさずレジストリを読む。読むのはフォルダ選びの箱を出す
// ときだけで、起動・開く・打鍵の道では呼ばない。
class WslDistributionPort
{
  public:
    WslDistributionPort() = default;
    virtual ~WslDistributionPort() = default;
    WslDistributionPort(const WslDistributionPort &) = delete;
    WslDistributionPort(WslDistributionPort &&) = delete;
    WslDistributionPort &operator=(const WslDistributionPort &) = delete;
    WslDistributionPort &operator=(WslDistributionPort &&) = delete;

    [[nodiscard]] virtual std::expected<std::vector<core::WslDistribution>, WslListFailure>
    distributions() const = 0;
};
} // namespace nenenib::application
