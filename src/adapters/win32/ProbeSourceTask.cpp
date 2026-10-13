#include "ProbeSourceTask.hpp"
#include "CompleteFileRead.hpp"
#include <algorithm>
#include <string>
#include <utility>
namespace nenenib::adapters::win32
{
ProbeSourceTask::ProbeSourceTask(ProbeSourceTask &&other) noexcept
    : source_(std::move(other.source_)), spool_(std::move(other.spool_)), length_(other.length_),
      io_(std::move(other.io_)), copied_(other.copied_),
      phase_(std::exchange(other.phase_, ProbeSourcePhase::handed_off))
{
}
ProbeSourceTask &ProbeSourceTask::operator=(ProbeSourceTask &&other) noexcept
{
    source_ = std::move(other.source_);
    spool_ = std::move(other.spool_);
    length_ = other.length_;
    io_ = std::move(other.io_);
    copied_ = other.copied_;
    phase_ = std::exchange(other.phase_, ProbeSourcePhase::handed_off);
    return *this;
}
ProbeSourceTask::ProbeSourceTask(std::unique_ptr<FileHandle> source,
                                 std::shared_ptr<FileHandle> spool, std::size_t length,
                                 std::shared_ptr<ProbeSourceIo> io)
    : source_(std::move(source)), spool_(std::move(spool)), length_(length), io_(std::move(io)),
      phase_(length == 0 ? ProbeSourcePhase::ready : ProbeSourcePhase::copying)
{
    if (length == 0)
    {
        source_.reset();
    }
}
std::expected<ProbeSourceTask, ProbeSourceFailure>
ProbeSourceTask::start(std::wstring_view source, std::wstring_view spool,
                       std::shared_ptr<ProbeSourceIo> io)
{
    if (!io)
    {
        return std::unexpected(ProbeSourceFailure::unavailable);
    }
    const std::wstring source_path(source);
    auto input = std::make_unique<FileHandle>(CreateFileW(source_path.c_str(), GENERIC_READ,
                                                          FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                                          FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!input->valid())
    {
        return std::unexpected(ProbeSourceFailure::unreadable);
    }
    const auto length = io->size(input->get());
    if (!length)
    {
        return std::unexpected(ProbeSourceFailure::unreadable);
    }
    if (length.value() > probe_source_limit_bytes)
    {
        return std::unexpected(ProbeSourceFailure::too_large);
    }
    const std::wstring spool_path(spool);
    const auto handle = CreateFileW(spool_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    const auto error = GetLastError();
    auto output = std::make_shared<FileHandle>(handle);
    if (!output->valid())
    {
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
        {
            return std::unexpected(ProbeSourceFailure::already_exists);
        }
        return std::unexpected(ProbeSourceFailure::create_failed);
    }
    return ProbeSourceTask(std::move(input), std::move(output), length.value(), std::move(io));
}
std::expected<void, ProbeSourceFailure> ProbeSourceTask::copy_step(std::size_t count)
{
    const auto bytes = complete_file_read(count, [&](std::span<char> target) noexcept
                                          { return io_->read(source_->get(), target); });
    if (!bytes)
    {
        return std::unexpected(ProbeSourceFailure::unreadable);
    }
    const auto written = io_->write(spool_->get(), std::span<const char>(bytes.value()));
    if (!written || written.value() != count)
    {
        return std::unexpected(ProbeSourceFailure::unwritable);
    }
    copied_ += count;
    return {};
}
std::expected<ProbeSourcePhase, ProbeSourceFailure> ProbeSourceTask::step(std::size_t budget)
{
    if (phase_ == ProbeSourcePhase::cancelled)
    {
        return std::unexpected(ProbeSourceFailure::cancelled);
    }
    if (!spool_)
    {
        return std::unexpected(ProbeSourceFailure::unavailable);
    }
    if (phase_ == ProbeSourcePhase::ready)
    {
        return phase_;
    }
    if (budget == 0 || budget > probe_source_block_bytes)
    {
        close(ProbeSourcePhase::failed);
        return std::unexpected(ProbeSourceFailure::invalid_range);
    }
    const auto copied = copy_step((std::min)(budget, length_ - copied_));
    if (!copied)
    {
        close(ProbeSourcePhase::failed);
        return std::unexpected(copied.error());
    }
    if (copied_ == length_)
    {
        source_.reset();
        phase_ = ProbeSourcePhase::ready;
    }
    return phase_;
}
std::expected<ProbeSealedSource, ProbeSourceFailure> ProbeSourceTask::take()
{
    if (phase_ == ProbeSourcePhase::cancelled)
    {
        return std::unexpected(ProbeSourceFailure::cancelled);
    }
    if (!spool_)
    {
        return std::unexpected(ProbeSourceFailure::unavailable);
    }
    if (phase_ != ProbeSourcePhase::ready)
    {
        return std::unexpected(ProbeSourceFailure::not_ready);
    }
    phase_ = ProbeSourcePhase::handed_off;
    return ProbeSealedSource(std::move(spool_), length_, io_);
}
void ProbeSourceTask::close(ProbeSourcePhase phase) noexcept
{
    source_.reset();
    spool_.reset();
    phase_ = phase;
}
void ProbeSourceTask::cancel() noexcept
{
    switch (phase_)
    {
    case ProbeSourcePhase::copying:
    case ProbeSourcePhase::ready:
        close(ProbeSourcePhase::cancelled);
        return;
    case ProbeSourcePhase::handed_off:
    case ProbeSourcePhase::cancelled:
    case ProbeSourcePhase::failed:
        return;
    }
    std::unreachable();
}
} // namespace nenenib::adapters::win32
