#pragma once
#include "ProbeReadTransport.hpp"
namespace nenenib::adapters::win32
{
class ProbeNativeReadTransport final : public ProbeReadTransport
{
  private:
    [[nodiscard]] ProbeIoReply issue_native(ProbeOverlappedStorage &storage) override;
    [[nodiscard]] ProbeIoReply query_native(ProbeOverlappedStorage &storage,
                                            ProbeQueryWait wait) override;
    [[nodiscard]] std::expected<void, std::uint32_t>
    cancel_native(ProbeOverlappedStorage &storage) override;
};
} // namespace nenenib::adapters::win32
