#include "PipeRead.hpp"

#include "OverlappedDrain.hpp"

#include <algorithm>
#include <limits>

namespace nenenib::adapters::win32
{
PipeRead::PipeRead(HANDLE pipe, std::size_t bytes)
    : pipe_(pipe), event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      buffer_(std::clamp<std::size_t>(bytes, 1, std::numeric_limits<DWORD>::max()))
{
}

PipeRead::~PipeRead()
{
    drain();
}

bool PipeRead::valid() const noexcept
{
    return event_.valid();
}

bool PipeRead::pending() const noexcept
{
    return pending_;
}

HANDLE PipeRead::event() const noexcept
{
    return event_.get();
}

std::expected<void, DWORD> PipeRead::issue()
{
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_.get();
    if (ReadFile(pipe_, buffer_.data(), static_cast<DWORD>(buffer_.size()), nullptr,
                 &overlapped_) != 0)
    {
        pending_ = true;
        return {};
    }
    const DWORD error = GetLastError();
    if (error == ERROR_IO_PENDING)
    {
        pending_ = true;
        return {};
    }
    return std::unexpected(error);
}

std::expected<std::string_view, DWORD> PipeRead::finish()
{
    DWORD transferred = 0;
    if (GetOverlappedResult(pipe_, &overlapped_, &transferred, FALSE) != 0)
    {
        pending_ = false;
        return std::string_view(buffer_.data(), transferred);
    }
    const DWORD error = GetLastError();
    pending_ = error == ERROR_IO_INCOMPLETE;
    return std::unexpected(error);
}

void PipeRead::drain() noexcept
{
    if (pending_)
    {
        drain_overlapped(pipe_, overlapped_);
        pending_ = false;
    }
}
} // namespace nenenib::adapters::win32
