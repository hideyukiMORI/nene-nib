#pragma once
#include "ProbeIoReply.hpp"
#include "ProbeQueryWait.hpp"
#include "ProbeReadHook.hpp"
#include <array>
#include <cstddef>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>
namespace nenenib::adapters::win32
{
class ProbeOverlappedStorage;
class ProbeReadTransport
{
  public:
    virtual ~ProbeReadTransport() = default;
    [[nodiscard]] ProbeIoReply issue(ProbeOverlappedStorage &storage);
    [[nodiscard]] ProbeIoReply query(ProbeOverlappedStorage &storage, ProbeQueryWait wait);
    [[nodiscard]] std::expected<void, std::uint32_t> cancel(ProbeOverlappedStorage &storage);
    void set_hook(ProbeReadHook boundary, std::function<void()> action);
    void hook(ProbeReadHook boundary);
    [[nodiscard]] std::size_t issues() const;
    [[nodiscard]] std::size_t queries() const;
    [[nodiscard]] std::size_t cancels() const;
    [[nodiscard]] std::vector<std::expected<void, std::uint32_t>> cancel_trace() const;
    [[nodiscard]] std::optional<ProbeIoReply> issue_reply() const;
    [[nodiscard]] std::vector<ProbeQueryWait> query_waits() const;

  private:
    [[nodiscard]] virtual ProbeIoReply issue_native(ProbeOverlappedStorage &storage) = 0;
    [[nodiscard]] virtual ProbeIoReply query_native(ProbeOverlappedStorage &storage,
                                                    ProbeQueryWait wait) = 0;
    [[nodiscard]] virtual std::expected<void, std::uint32_t>
    cancel_native(ProbeOverlappedStorage &storage) = 0;
    mutable std::mutex mutex_;
    std::array<std::function<void()>, 7> hooks_;
    std::size_t issues_ = 0, queries_ = 0, cancels_ = 0;
    std::vector<std::expected<void, std::uint32_t>> trace_;
    std::optional<ProbeIoReply> issue_reply_;
    std::vector<ProbeQueryWait> query_waits_;
};
} // namespace nenenib::adapters::win32
