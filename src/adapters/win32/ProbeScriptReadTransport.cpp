#include "ProbeScriptReadTransport.hpp"
#include "ProbeOverlappedStorage.hpp"
#include <algorithm>
#include <exception>
#include <utility>
namespace nenenib::adapters::win32
{
ProbeScriptReadTransport::ProbeScriptReadTransport(
    ProbeIoReply issue, std::vector<ProbeIoReply> queries,
    std::vector<std::expected<void, std::uint32_t>> cancels, std::string bytes)
    : issue_(issue), queries_(std::move(queries)), cancels_(std::move(cancels)),
      bytes_(std::move(bytes))
{
}
ProbeIoReply ProbeScriptReadTransport::issue_native(ProbeOverlappedStorage &storage)
{
    if (bytes_.size() > storage.count_)
    {
        std::terminate();
    }
    std::copy(bytes_.begin(), bytes_.end(), storage.buffer_.begin());
    return issue_;
}
ProbeIoReply ProbeScriptReadTransport::query_native(ProbeOverlappedStorage &, ProbeQueryWait)
{
    if (query_at_ >= queries_.size())
    {
        std::terminate();
    }
    return queries_[query_at_++];
}
std::expected<void, std::uint32_t> ProbeScriptReadTransport::cancel_native(ProbeOverlappedStorage &)
{
    const std::scoped_lock lock(cancel_mutex_);
    if (cancel_at_ >= cancels_.size())
    {
        std::terminate();
    }
    return cancels_[cancel_at_++];
}
} // namespace nenenib::adapters::win32
