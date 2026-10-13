#include "ProbePipeFixture.hpp"
#include <exception>
namespace nenenib::adapters::win32
{
namespace
{
constexpr wchar_t pipe_name[] = L"\\\\.\\pipe\\NeNeNib-467-overlapped-cancel-probe";
[[nodiscard]] bool drain_connect(HANDLE server, OVERLAPPED &overlapped)
{
    DWORD ignored = 0;
    if (GetOverlappedResult(server, &overlapped, &ignored, TRUE) == 0)
    {
        const auto error = GetLastError();
        if (error == ERROR_INVALID_HANDLE || error == ERROR_INVALID_PARAMETER ||
            error == ERROR_IO_INCOMPLETE)
        {
            std::terminate();
        }
        return false;
    }
    return true;
}
} // namespace
std::expected<ProbePipeFixture, ProbeReadFailure> ProbePipeFixture::create()
{
    auto server = std::make_unique<FileHandle>(CreateNamedPipeW(
        pipe_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 64, 64, 0,
        nullptr));
    if (!server->valid())
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    const auto raw_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (raw_event == nullptr)
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    const FileHandle event(raw_event);
    OVERLAPPED connect{};
    connect.hEvent = event.get();
    const auto connected = ConnectNamedPipe(server->get(), &connect);
    const auto error = GetLastError();
    const bool pending = connected == 0 && error == ERROR_IO_PENDING;
    if (connected == 0 && !pending && error != ERROR_PIPE_CONNECTED)
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    auto client = std::make_unique<FileHandle>(CreateFileW(
        pipe_name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!client->valid())
    {
        if (pending)
        {
            static_cast<void>(CancelIoEx(server->get(), &connect));
            static_cast<void>(drain_connect(server->get(), connect));
        }
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    if (pending && !drain_connect(server->get(), connect))
    {
        return std::unexpected(ProbeReadFailure::create_failed);
    }
    return ProbePipeFixture(std::move(server), std::move(client));
}
bool ProbePipeFixture::write(std::string_view bytes)
{
    if (!writer_ || bytes.size() > 64)
    {
        return false;
    }
    DWORD done = 0;
    return WriteFile(writer_->get(), bytes.data(), static_cast<DWORD>(bytes.size()), &done,
                     nullptr) != 0 &&
           done == bytes.size();
}
} // namespace nenenib::adapters::win32
