#include "ProbeSourceIo.hpp"
#include <algorithm>
#include <utility>
namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] ProbeIoFault next_fault(const std::vector<ProbeIoFault> &faults, std::size_t &at)
{
    if (at < faults.size())
    {
        return faults[at++];
    }
    return ProbeIoFault::normal;
}
[[nodiscard]] std::size_t operation_size(ProbeIoFault fault, std::size_t requested) noexcept
{
    switch (fault)
    {
    case ProbeIoFault::normal:
        return requested;
    case ProbeIoFault::short_io:
        return requested / 2;
    case ProbeIoFault::zero:
        return 0;
    case ProbeIoFault::failed:
        return 0;
    }
    std::unreachable();
}
} // namespace
void ProbeSourceIo::script_reads(std::vector<ProbeIoFault> faults)
{
    reads_ = std::move(faults);
    read_at_ = 0;
}
void ProbeSourceIo::script_writes(std::vector<ProbeIoFault> faults)
{
    writes_ = std::move(faults);
    write_at_ = 0;
}
void ProbeSourceIo::script_size(std::size_t declared_size)
{
    size_ = declared_size;
}
std::expected<std::size_t, application::FileFailure> ProbeSourceIo::size(HANDLE file) const
{
    if (size_.has_value())
    {
        return size_.value();
    }
    LARGE_INTEGER length{};
    if (GetFileSizeEx(file, &length) == 0 || length.QuadPart < 0)
    {
        return std::unexpected(application::FileFailure::unreadable);
    }
    return static_cast<std::size_t>(length.QuadPart);
}
std::expected<std::size_t, application::FileFailure>
ProbeSourceIo::read(HANDLE file, std::span<char> target) noexcept
{
    const auto fault = next_fault(reads_, read_at_);
    if (fault == ProbeIoFault::failed)
    {
        return std::unexpected(application::FileFailure::unreadable);
    }
    const auto requested = operation_size(fault, target.size());
    if (requested == 0)
    {
        return std::size_t{0};
    }
    DWORD done = 0;
    if (ReadFile(file, target.data(), static_cast<DWORD>(requested), &done, nullptr) == 0)
    {
        return std::unexpected(application::FileFailure::unreadable);
    }
    return static_cast<std::size_t>(done);
}
std::expected<std::size_t, application::FileFailure>
ProbeSourceIo::write(HANDLE file, std::span<const char> bytes) noexcept
{
    const auto fault = next_fault(writes_, write_at_);
    if (fault == ProbeIoFault::failed)
    {
        return std::unexpected(application::FileFailure::unwritable);
    }
    const auto requested = operation_size(fault, bytes.size());
    DWORD done = 0;
    if (WriteFile(file, bytes.data(), static_cast<DWORD>(requested), &done, nullptr) == 0)
    {
        return std::unexpected(application::FileFailure::unwritable);
    }
    return static_cast<std::size_t>(done);
}
} // namespace nenenib::adapters::win32
