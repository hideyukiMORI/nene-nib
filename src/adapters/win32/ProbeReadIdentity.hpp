#pragma once
#include <cstddef>
#include <memory>
namespace nenenib::adapters::win32
{
class ProbeReadIdentity final
{
  public:
    ProbeReadIdentity() : token_(std::make_shared<const std::byte>()) {}
    [[nodiscard]] bool same(const ProbeReadIdentity &other) const noexcept
    {
        return token_ == other.token_;
    }

  private:
    std::shared_ptr<const std::byte> token_;
};
} // namespace nenenib::adapters::win32
