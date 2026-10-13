#pragma once
#include "FileHandle.hpp"
#include "ProbeReadState.hpp"
#include "ProbeSourceBytes.hpp"
#include "ProbeSourceFailure.hpp"
#include "ProbeSourceIo.hpp"
#include <cstddef>
#include <expected>
#include <memory>
namespace nenenib::adapters::win32
{
class ProbeRangeRead final
{
  public:
    ProbeRangeRead(const ProbeRangeRead &) = delete;
    ProbeRangeRead &operator=(const ProbeRangeRead &) = delete;
    ProbeRangeRead(ProbeRangeRead &&other) noexcept;
    ProbeRangeRead &operator=(ProbeRangeRead &&other) noexcept;
    [[nodiscard]] std::expected<ProbeSourceBytes, ProbeSourceFailure> run();
    void cancel() noexcept;

  private:
    friend class ProbeSealedSource;
    ProbeRangeRead(ProbeSourceId id, std::shared_ptr<FileHandle> file,
                   std::shared_ptr<ProbeSourceIo> io);
    ProbeSourceId id_;
    std::shared_ptr<FileHandle> file_;
    std::shared_ptr<ProbeSourceIo> io_;
    std::size_t offset_ = 0;
    std::size_t count_ = 0;
    ProbeReadState state_ = ProbeReadState::pending;
};
} // namespace nenenib::adapters::win32
