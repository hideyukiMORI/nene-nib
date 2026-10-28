#include "PipeWrite.hpp"

#include "OverlappedDrain.hpp"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

namespace nenenib::adapters::win32
{
PipeWrite::PipeWrite(HANDLE pipe) : pipe_(pipe), event_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
{
}

PipeWrite::~PipeWrite()
{
    drain();
}

bool PipeWrite::valid() const noexcept
{
    return event_.valid();
}

bool PipeWrite::pending() const noexcept
{
    return pending_;
}

HANDLE PipeWrite::event() const noexcept
{
    return event_.get();
}

std::expected<void, DWORD> PipeWrite::push(std::string frame)
{
    queue_.push_back(std::move(frame));
    if (pending_)
    {
        return {};
    }
    return next();
}

std::expected<void, DWORD> PipeWrite::finish()
{
    DWORD transferred = 0;
    if (GetOverlappedResult(pipe_, &overlapped_, &transferred, FALSE) == 0)
    {
        const DWORD error = GetLastError();
        pending_ = error == ERROR_IO_INCOMPLETE;
        return std::unexpected(error);
    }
    pending_ = false;
    written_ += transferred;
    if (written_ < current_.size())
    {
        return begin();
    }
    return next();
}

void PipeWrite::drain() noexcept
{
    if (pending_)
    {
        drain_overlapped(pipe_, overlapped_);
        pending_ = false;
    }
    queue_.clear();
    current_.clear();
    written_ = 0;
}

std::expected<void, DWORD> PipeWrite::next()
{
    current_.clear();
    written_ = 0;
    if (queue_.empty())
    {
        return {};
    }
    current_ = std::move(queue_.front());
    queue_.pop_front();
    return begin();
}

// 1 回の WriteFile は DWORD に収まる分だけ。残りは完了の後に続きを書く。
std::expected<void, DWORD> PipeWrite::begin()
{
    const std::string_view rest = std::string_view(current_).substr(written_);
    const auto size =
        static_cast<DWORD>(std::min<std::size_t>(rest.size(), std::numeric_limits<DWORD>::max()));
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_.get();
    if (WriteFile(pipe_, rest.data(), size, nullptr, &overlapped_) != 0)
    {
        pending_ = true;
        return {};
    }
    const DWORD error = GetLastError();
    pending_ = error == ERROR_IO_PENDING;
    if (pending_)
    {
        return {};
    }
    return std::unexpected(error);
}
} // namespace nenenib::adapters::win32
