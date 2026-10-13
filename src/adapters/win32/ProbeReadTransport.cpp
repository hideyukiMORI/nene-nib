#include "ProbeReadTransport.hpp"
#include <utility>
namespace nenenib::adapters::win32
{
ProbeIoReply ProbeReadTransport::issue(ProbeOverlappedStorage &storage)
{
    {
        const std::scoped_lock lock(mutex_);
        ++issues_;
    }
    hook(ProbeReadHook::issue);
    auto reply = issue_native(storage);
    {
        const std::scoped_lock lock(mutex_);
        issue_reply_ = reply;
    }
    return reply;
}
ProbeIoReply ProbeReadTransport::query(ProbeOverlappedStorage &storage, ProbeQueryWait wait)
{
    {
        const std::scoped_lock lock(mutex_);
        ++queries_;
        query_waits_.push_back(wait);
    }
    hook(ProbeReadHook::query);
    return query_native(storage, wait);
}
std::expected<void, std::uint32_t> ProbeReadTransport::cancel(ProbeOverlappedStorage &storage)
{
    {
        const std::scoped_lock lock(mutex_);
        ++cancels_;
    }
    hook(ProbeReadHook::cancel);
    auto result = cancel_native(storage);
    {
        const std::scoped_lock lock(mutex_);
        trace_.push_back(result);
    }
    hook(ProbeReadHook::cancel_return);
    return result;
}
void ProbeReadTransport::set_hook(ProbeReadHook boundary, std::function<void()> action)
{
    const std::scoped_lock lock(mutex_);
    hooks_[static_cast<std::size_t>(boundary)] = std::move(action);
}
void ProbeReadTransport::hook(ProbeReadHook boundary)
{
    std::function<void()> action;
    {
        const std::scoped_lock lock(mutex_);
        action = hooks_[static_cast<std::size_t>(boundary)];
    }
    if (action)
    {
        action();
    }
}
std::size_t ProbeReadTransport::issues() const
{
    const std::scoped_lock lock(mutex_);
    return issues_;
}
std::size_t ProbeReadTransport::queries() const
{
    const std::scoped_lock lock(mutex_);
    return queries_;
}
std::size_t ProbeReadTransport::cancels() const
{
    const std::scoped_lock lock(mutex_);
    return cancels_;
}
std::vector<std::expected<void, std::uint32_t>> ProbeReadTransport::cancel_trace() const
{
    const std::scoped_lock lock(mutex_);
    return trace_;
}
std::optional<ProbeIoReply> ProbeReadTransport::issue_reply() const
{
    const std::scoped_lock lock(mutex_);
    return issue_reply_;
}
std::vector<ProbeQueryWait> ProbeReadTransport::query_waits() const
{
    const std::scoped_lock lock(mutex_);
    return query_waits_;
}
} // namespace nenenib::adapters::win32
