#pragma once
#include "ProbeReadControl.hpp"
#include <memory>
namespace nenenib::adapters::win32
{
class ProbeReadTicket final
{
  public:
    ProbeReadTicket(const ProbeReadTicket &) = default;
    ProbeReadTicket &operator=(const ProbeReadTicket &) = default;
    ProbeReadTicket(ProbeReadTicket &&other) noexcept : control_(other.control_) {}
    ProbeReadTicket &operator=(ProbeReadTicket &&other) noexcept
    {
        control_ = other.control_;
        return *this;
    }
    [[nodiscard]] ProbeCancelKind cancel() const
    {
        return control_->cancel();
    }
    [[nodiscard]] std::expected<ProbeReadResult, ProbeReadFailure> collect() const
    {
        return control_->collect();
    }
    [[nodiscard]] bool storage_alive() const
    {
        return control_->storage_alive();
    }

  public:
    [[nodiscard]] ProbeReadIdentity identity() const
    {
        return control_->identity();
    }

  private:
    friend class ProbeReadOwner;
    explicit ProbeReadTicket(std::shared_ptr<ProbeReadControl> control)
        : control_(std::move(control))
    {
    }
    std::shared_ptr<ProbeReadControl> control_;
};
} // namespace nenenib::adapters::win32
