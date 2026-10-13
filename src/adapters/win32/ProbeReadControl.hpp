#pragma once
#include "ProbeCancelKind.hpp"
#include "ProbeOverlappedStorage.hpp"
#include "ProbeReadPhase.hpp"
#include "ProbeReadResult.hpp"
#include "ProbeReadTransport.hpp"
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
namespace nenenib::adapters::win32
{
class ProbeReadControl final
{
  public:
    explicit ProbeReadControl(std::shared_ptr<ProbeReadTransport> transport)
        : transport_(std::move(transport))
    {
    }
    [[nodiscard]] ProbeCancelKind cancel();
    [[nodiscard]] std::expected<ProbeReadResult, ProbeReadFailure> collect();
    [[nodiscard]] bool storage_alive() const;
    [[nodiscard]] ProbeReadIdentity identity() const
    {
        return identity_;
    }

  private:
    friend class ProbeReadOwner;
    void execute(const std::shared_ptr<ProbeOverlappedStorage> &storage);
    [[nodiscard]] bool begin();
    [[nodiscard]] bool pending();
    [[nodiscard]] ProbeIoReply drain(ProbeOverlappedStorage &storage, ProbeIoReply issue);
    void publish(ProbeOverlappedStorage &storage, ProbeIoReply reply);
    ProbeReadIdentity identity_;
    mutable std::mutex mutex_;
    ProbeReadPhase phase_ = ProbeReadPhase::queued;
    bool cancelled_ = false, collected_ = false;
    std::weak_ptr<ProbeOverlappedStorage> storage_;
    std::weak_ptr<ProbeOverlappedStorage> observation_;
    std::optional<ProbeReadResult> result_;
    std::shared_ptr<ProbeReadTransport> transport_;
};
} // namespace nenenib::adapters::win32
