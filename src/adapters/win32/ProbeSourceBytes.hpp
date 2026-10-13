#pragma once
#include "ProbeSourceId.hpp"
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
namespace nenenib::adapters::win32
{
class ProbeSourceBytes final
{
  public:
    [[nodiscard]] const ProbeSourceId &id() const & noexcept
    {
        return id_;
    }
    const ProbeSourceId &id() const && = delete;
    [[nodiscard]] std::size_t offset() const noexcept
    {
        return offset_;
    }
    [[nodiscard]] std::string_view bytes() const & noexcept
    {
        return bytes_;
    }
    std::string_view bytes() const && = delete;

  private:
    friend class ProbeRangeRead;
    ProbeSourceBytes(ProbeSourceId id, std::size_t offset, std::string_view bytes)
        : id_(std::move(id)), offset_(offset), bytes_(bytes)
    {
    }
    ProbeSourceId id_;
    std::size_t offset_;
    std::string bytes_;
};
} // namespace nenenib::adapters::win32
