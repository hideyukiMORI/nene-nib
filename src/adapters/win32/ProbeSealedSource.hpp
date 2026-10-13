#pragma once
#include "ProbeRangeRead.hpp"
#include <cstddef>
#include <expected>
#include <memory>
namespace nenenib::adapters::win32
{
inline constexpr std::size_t probe_source_block_bytes = 1024U * 1024U;
inline constexpr std::size_t probe_source_limit_bytes = 4U * probe_source_block_bytes;
class ProbeSealedSource final
{
  public:
    ProbeSealedSource(const ProbeSealedSource &) = delete;
    ProbeSealedSource &operator=(const ProbeSealedSource &) = delete;
    ProbeSealedSource(ProbeSealedSource &&) = default;
    ProbeSealedSource &operator=(ProbeSealedSource &&) = default;
    [[nodiscard]] std::size_t length() const noexcept
    {
        return length_;
    }
    [[nodiscard]] const ProbeSourceId &id() const & noexcept
    {
        return id_;
    }
    const ProbeSourceId &id() const && = delete;
    [[nodiscard]] std::expected<ProbeRangeRead, ProbeSourceFailure>
    request(std::size_t offset, std::size_t count) const;
    void release() noexcept;

  private:
    friend class ProbeSourceTask;
    ProbeSealedSource(std::shared_ptr<FileHandle> file, std::size_t length,
                      std::shared_ptr<ProbeSourceIo> io);
    ProbeSourceId id_;
    std::shared_ptr<FileHandle> file_;
    std::size_t length_;
    std::shared_ptr<ProbeSourceIo> io_;
};
} // namespace nenenib::adapters::win32
