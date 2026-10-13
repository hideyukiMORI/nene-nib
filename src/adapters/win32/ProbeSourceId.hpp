#pragma once
#include <cstddef>
#include <memory>
namespace nenenib::adapters::win32
{
class ProbeSourceId final
{
  public:
    ProbeSourceId(const ProbeSourceId &) = default;
    ProbeSourceId &operator=(const ProbeSourceId &) = default;
    ProbeSourceId(ProbeSourceId &&other) noexcept : token_(other.token_) {}
    ProbeSourceId &operator=(ProbeSourceId &&other) noexcept
    {
        token_ = other.token_;
        return *this;
    }
    [[nodiscard]] static ProbeSourceId fresh()
    {
        return ProbeSourceId();
    }
    [[nodiscard]] bool same(const ProbeSourceId &other) const noexcept
    {
        return token_ == other.token_;
    }

  private:
    ProbeSourceId() : token_(std::make_shared<const std::byte>()) {}
    std::shared_ptr<const std::byte> token_;
};
} // namespace nenenib::adapters::win32
