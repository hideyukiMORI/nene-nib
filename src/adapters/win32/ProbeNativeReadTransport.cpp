#include "ProbeNativeReadTransport.hpp"
#include "ProbeOverlappedStorage.hpp"
#include <utility>
namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] BOOL native_wait(ProbeQueryWait wait) noexcept
{
    switch (wait)
    {
    case ProbeQueryWait::poll:
        return FALSE;
    case ProbeQueryWait::wait:
        return TRUE;
    }
    std::unreachable();
}
} // namespace
ProbeIoReply ProbeNativeReadTransport::issue_native(ProbeOverlappedStorage &storage)
{
    if (ReadFile(storage.file_->get(), storage.buffer_.data(), static_cast<DWORD>(storage.count_),
                 nullptr, &storage.overlapped_) != 0)
    {
        return ProbeIoReply(ProbeReplyKind::immediate);
    }
    const auto error = GetLastError();
    if (error == ERROR_IO_PENDING)
    {
        return ProbeIoReply(ProbeReplyKind::pending, error);
    }
    return ProbeIoReply(ProbeReplyKind::failed, error);
}
ProbeIoReply ProbeNativeReadTransport::query_native(ProbeOverlappedStorage &storage,
                                                    ProbeQueryWait wait)
{
    DWORD done = 0;
    if (GetOverlappedResult(storage.file_->get(), &storage.overlapped_, &done, native_wait(wait)) !=
        0)
    {
        return ProbeIoReply(ProbeReplyKind::complete, 0, done);
    }
    const auto error = GetLastError();
    if (error == ERROR_IO_INCOMPLETE)
    {
        return ProbeIoReply(ProbeReplyKind::incomplete, error);
    }
    return ProbeIoReply(ProbeReplyKind::failed, error);
}
std::expected<void, std::uint32_t>
ProbeNativeReadTransport::cancel_native(ProbeOverlappedStorage &storage)
{
    if (CancelIoEx(storage.file_->get(), &storage.overlapped_) == 0)
    {
        return std::unexpected(GetLastError());
    }
    return {};
}
} // namespace nenenib::adapters::win32
