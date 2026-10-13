#pragma once
#include "ProbeReadTransport.hpp"
#include <string>
#include <vector>
namespace nenenib::adapters::win32
{
class ProbeScriptReadTransport final : public ProbeReadTransport
{
  public:
    ProbeScriptReadTransport(ProbeIoReply issue, std::vector<ProbeIoReply> queries,
                             std::vector<std::expected<void, std::uint32_t>> cancels,
                             std::string bytes);

  private:
    [[nodiscard]] ProbeIoReply issue_native(ProbeOverlappedStorage &storage) override;
    [[nodiscard]] ProbeIoReply query_native(ProbeOverlappedStorage &storage,
                                            ProbeQueryWait wait) override;
    [[nodiscard]] std::expected<void, std::uint32_t>
    cancel_native(ProbeOverlappedStorage &storage) override;
    ProbeIoReply issue_;
    std::vector<ProbeIoReply> queries_;
    std::vector<std::expected<void, std::uint32_t>> cancels_;
    std::string bytes_;
    std::size_t query_at_ = 0, cancel_at_ = 0;
    std::mutex cancel_mutex_;
};
} // namespace nenenib::adapters::win32
