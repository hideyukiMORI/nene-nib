#pragma once
#include "ProbeIoReply.hpp"
#include "ProbeReadApplication.hpp"
#include "ProbeReadIdentity.hpp"
#include <string>
#include <string_view>
#include <utility>
namespace nenenib::adapters::win32
{
class ProbeReadResult final
{
  public:
    [[nodiscard]] ProbeIoReply os() const noexcept
    {
        return os_;
    }
    [[nodiscard]] ProbeReadApplication application() const noexcept
    {
        return application_;
    }
    [[nodiscard]] const ProbeReadIdentity &identity() const & noexcept
    {
        return identity_;
    }
    const ProbeReadIdentity &identity() const && = delete;
    [[nodiscard]] std::string_view bytes() const & noexcept
    {
        return bytes_;
    }
    std::string_view bytes() const && = delete;

  private:
    friend class ProbeReadControl;
    ProbeReadResult(ProbeReadIdentity identity, ProbeIoReply os, ProbeReadApplication application,
                    std::string bytes)
        : identity_(std::move(identity)), os_(os), application_(application),
          bytes_(std::move(bytes))
    {
    }
    ProbeReadIdentity identity_;
    ProbeIoReply os_;
    ProbeReadApplication application_;
    std::string bytes_;
};
} // namespace nenenib::adapters::win32
