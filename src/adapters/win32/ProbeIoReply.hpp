#pragma once
#include "ProbeReplyKind.hpp"
#include <cstddef>
#include <cstdint>
namespace nenenib::adapters::win32
{
class ProbeIoReply final
{
  public:
    ProbeIoReply(ProbeReplyKind kind, std::uint32_t error = 0, std::size_t bytes = 0)
        : kind_(kind), error_(error), bytes_(bytes)
    {
    }
    [[nodiscard]] ProbeReplyKind kind() const noexcept
    {
        return kind_;
    }
    [[nodiscard]] std::uint32_t error() const noexcept
    {
        return error_;
    }
    [[nodiscard]] std::size_t bytes() const noexcept
    {
        return bytes_;
    }

  private:
    ProbeReplyKind kind_;
    std::uint32_t error_;
    std::size_t bytes_;
};
} // namespace nenenib::adapters::win32
