#include "ProbeReadControl.hpp"
#include <exception>
#include <string>
#include <utility>
namespace nenenib::adapters::win32
{
namespace
{
void require_valid(ProbeIoReply reply)
{
    if (reply.error() == ERROR_INVALID_HANDLE || reply.error() == ERROR_INVALID_PARAMETER)
    {
        std::terminate();
    }
}
void require_issue(ProbeIoReply reply)
{
    require_valid(reply);
    if (reply.kind() == ProbeReplyKind::failed &&
        (reply.error() == ERROR_IO_PENDING || reply.error() == ERROR_IO_INCOMPLETE))
    {
        std::terminate();
    }
    switch (reply.kind())
    {
    case ProbeReplyKind::immediate:
    case ProbeReplyKind::pending:
    case ProbeReplyKind::failed:
        return;
    case ProbeReplyKind::complete:
    case ProbeReplyKind::incomplete:
    case ProbeReplyKind::not_issued:
        std::terminate();
    }
    std::unreachable();
}
void require_terminal(ProbeIoReply reply)
{
    require_valid(reply);
    if (reply.error() == ERROR_IO_INCOMPLETE || reply.error() == ERROR_IO_PENDING)
    {
        std::terminate();
    }
    switch (reply.kind())
    {
    case ProbeReplyKind::complete:
    case ProbeReplyKind::failed:
        return;
    case ProbeReplyKind::immediate:
    case ProbeReplyKind::pending:
    case ProbeReplyKind::incomplete:
    case ProbeReplyKind::not_issued:
        std::terminate();
    }
    std::unreachable();
}
} // namespace
ProbeCancelKind ProbeReadControl::cancel()
{
    std::shared_ptr<ProbeOverlappedStorage> pin;
    {
        const std::scoped_lock lock(mutex_);
        if (phase_ == ProbeReadPhase::published)
        {
            return ProbeCancelKind::too_late;
        }
        if (cancelled_)
        {
            return ProbeCancelKind::already_requested;
        }
        cancelled_ = true;
        if (phase_ != ProbeReadPhase::queued)
        {
            pin = storage_.lock();
        }
    }
    if (pin)
    {
        static_cast<void>(transport_->cancel(*pin));
    }
    return ProbeCancelKind::accepted;
}
bool ProbeReadControl::begin()
{
    const std::scoped_lock lock(mutex_);
    if (cancelled_)
    {
        return false;
    }
    phase_ = ProbeReadPhase::issuing;
    return true;
}
bool ProbeReadControl::pending()
{
    const std::scoped_lock lock(mutex_);
    phase_ = ProbeReadPhase::pending;
    return cancelled_;
}
ProbeIoReply ProbeReadControl::drain(ProbeOverlappedStorage &storage, ProbeIoReply issue)
{
    require_issue(issue);
    if (issue.kind() == ProbeReplyKind::failed)
    {
        return issue;
    }
    if (issue.kind() == ProbeReplyKind::pending)
    {
        const bool rearm = pending();
        transport_->hook(ProbeReadHook::pending);
        if (rearm)
        {
            static_cast<void>(transport_->cancel(storage));
        }
    }
    auto result = transport_->query(storage, ProbeQueryWait::poll);
    require_valid(result);
    if (result.kind() == ProbeReplyKind::incomplete)
    {
        if (issue.kind() != ProbeReplyKind::pending)
        {
            std::terminate();
        }
        result = transport_->query(storage, ProbeQueryWait::wait);
        require_valid(result);
        if (result.kind() == ProbeReplyKind::incomplete)
        {
            std::terminate();
        }
    }
    require_terminal(result);
    return result;
}
void ProbeReadControl::publish(ProbeOverlappedStorage &storage, ProbeIoReply reply)
{
    std::string bytes;
    if (reply.kind() == ProbeReplyKind::complete)
    {
        if (reply.bytes() > storage.count_)
        {
            std::terminate();
        }
        bytes.assign(storage.buffer_.data(), reply.bytes());
    }
    transport_->hook(ProbeReadHook::before_publish);
    const std::scoped_lock lock(mutex_);
    ProbeReadApplication application = ProbeReadApplication::failed;
    if (reply.kind() == ProbeReplyKind::complete)
    {
        application = ProbeReadApplication::delivered;
    }
    if (cancelled_)
    {
        application = ProbeReadApplication::cancelled;
        bytes.clear();
    }
    result_ = ProbeReadResult(identity_, reply, application, std::move(bytes));
    phase_ = ProbeReadPhase::published;
    storage_.reset();
}
void ProbeReadControl::execute(const std::shared_ptr<ProbeOverlappedStorage> &storage)
{
    if (!begin())
    {
        publish(*storage, ProbeIoReply(ProbeReplyKind::not_issued));
        return;
    }
    publish(*storage, drain(*storage, transport_->issue(*storage)));
}
std::expected<ProbeReadResult, ProbeReadFailure> ProbeReadControl::collect()
{
    const std::scoped_lock lock(mutex_);
    if (collected_)
    {
        return std::unexpected(ProbeReadFailure::consumed);
    }
    if (!result_.has_value())
    {
        return std::unexpected(ProbeReadFailure::not_ready);
    }
    collected_ = true;
    return std::move(result_).value();
}
bool ProbeReadControl::storage_alive() const
{
    const std::scoped_lock lock(mutex_);
    return !observation_.expired();
}
} // namespace nenenib::adapters::win32
