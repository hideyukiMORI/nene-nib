#include "ProbeRangeRead.hpp"
#include "CompleteFileRead.hpp"
#include <utility>
namespace nenenib::adapters::win32
{
ProbeRangeRead::ProbeRangeRead(ProbeRangeRead &&other) noexcept
    : id_(other.id_), file_(std::move(other.file_)), io_(std::move(other.io_)),
      offset_(other.offset_), count_(other.count_),
      state_(std::exchange(other.state_, ProbeReadState::consumed))
{
}
ProbeRangeRead &ProbeRangeRead::operator=(ProbeRangeRead &&other) noexcept
{
    id_ = other.id_;
    file_ = std::move(other.file_);
    io_ = std::move(other.io_);
    offset_ = other.offset_;
    count_ = other.count_;
    state_ = std::exchange(other.state_, ProbeReadState::consumed);
    return *this;
}
ProbeRangeRead::ProbeRangeRead(ProbeSourceId id, std::shared_ptr<FileHandle> file,
                               std::shared_ptr<ProbeSourceIo> io)
    : id_(std::move(id)), file_(std::move(file)), io_(std::move(io))
{
}
std::expected<ProbeSourceBytes, ProbeSourceFailure> ProbeRangeRead::run()
{
    if (state_ == ProbeReadState::cancelled)
    {
        return std::unexpected(ProbeSourceFailure::cancelled);
    }
    if (state_ == ProbeReadState::consumed || !file_)
    {
        return std::unexpected(ProbeSourceFailure::consumed);
    }
    state_ = ProbeReadState::consumed;
    auto file = std::move(file_);
    LARGE_INTEGER offset{};
    offset.QuadPart = static_cast<LONGLONG>(offset_);
    if (SetFilePointerEx(file->get(), offset, nullptr, FILE_BEGIN) == 0)
    {
        return std::unexpected(ProbeSourceFailure::unreadable);
    }
    const auto bytes = complete_file_read(count_, [&](std::span<char> target) noexcept
                                          { return io_->read(file->get(), target); });
    if (!bytes)
    {
        return std::unexpected(ProbeSourceFailure::unreadable);
    }
    return ProbeSourceBytes(id_, offset_, bytes.value());
}
void ProbeRangeRead::cancel() noexcept
{
    if (state_ == ProbeReadState::pending)
    {
        state_ = ProbeReadState::cancelled;
    }
    file_.reset();
}
} // namespace nenenib::adapters::win32
