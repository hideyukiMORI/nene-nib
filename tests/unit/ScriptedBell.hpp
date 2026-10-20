#pragma once

#include "BellPort.hpp"

#include <cstddef>

namespace nenenib::tests
{
// 断りの音の替え玉。OS を呼ばずに鳴った回数だけを数える（ADR 0153 の決定 6）。
class ScriptedBell final : public nenenib::application::BellPort
{
  public:
    void ring() noexcept override
    {
        ++rings_;
    }

    [[nodiscard]] std::size_t rings() const noexcept
    {
        return rings_;
    }

  private:
    std::size_t rings_ = 0;
};
} // namespace nenenib::tests
