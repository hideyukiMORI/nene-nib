#pragma once
#include "ProbeSealedSource.hpp"
#include "ProbeSourcePhase.hpp"
#include <cstddef>
#include <expected>
#include <memory>
#include <string_view>
namespace nenenib::adapters::win32
{
class ProbeSourceTask final
{
  public:
    [[nodiscard]] static std::expected<ProbeSourceTask, ProbeSourceFailure>
    start(std::wstring_view source, std::wstring_view spool, std::shared_ptr<ProbeSourceIo> io);
    ProbeSourceTask(const ProbeSourceTask &) = delete;
    ProbeSourceTask &operator=(const ProbeSourceTask &) = delete;
    ProbeSourceTask(ProbeSourceTask &&other) noexcept;
    ProbeSourceTask &operator=(ProbeSourceTask &&other) noexcept;
    [[nodiscard]] std::expected<ProbeSourcePhase, ProbeSourceFailure> step(std::size_t budget);
    [[nodiscard]] std::expected<ProbeSealedSource, ProbeSourceFailure> take();
    void cancel() noexcept;

  private:
    ProbeSourceTask(std::unique_ptr<FileHandle> source, std::shared_ptr<FileHandle> spool,
                    std::size_t length, std::shared_ptr<ProbeSourceIo> io);
    [[nodiscard]] std::expected<void, ProbeSourceFailure> copy_step(std::size_t count);
    void close(ProbeSourcePhase phase) noexcept;
    std::unique_ptr<FileHandle> source_;
    std::shared_ptr<FileHandle> spool_;
    std::size_t length_;
    std::shared_ptr<ProbeSourceIo> io_;
    std::size_t copied_ = 0;
    ProbeSourcePhase phase_;
};
} // namespace nenenib::adapters::win32
