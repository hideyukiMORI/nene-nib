#include "ProcessInstances.hpp"

#include <windows.h>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] std::uint64_t ticks_of(const FILETIME &time) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

// 開いた process の作成時刻。終わっていれば gone、読めなければ denied。
[[nodiscard]] std::expected<std::uint64_t, ProcessQueryFailure>
created_of_open(HANDLE process) noexcept
{
    DWORD code = 0;
    if (GetExitCodeProcess(process, &code) == 0)
    {
        return std::unexpected(ProcessQueryFailure::denied);
    }
    if (code != STILL_ACTIVE)
    {
        return std::unexpected(ProcessQueryFailure::gone);
    }
    FILETIME created{};
    FILETIME exited{};
    FILETIME in_kernel{};
    FILETIME in_user{};
    if (GetProcessTimes(process, &created, &exited, &in_kernel, &in_user) == 0)
    {
        return std::unexpected(ProcessQueryFailure::denied);
    }
    return ticks_of(created);
}
} // namespace

std::expected<std::uint64_t, ProcessQueryFailure> process_created(std::uint32_t pid) noexcept
{
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
    {
        return std::unexpected(GetLastError() == ERROR_INVALID_PARAMETER
                                   ? ProcessQueryFailure::gone
                                   : ProcessQueryFailure::denied);
    }
    auto created = created_of_open(process);
    static_cast<void>(CloseHandle(process));
    return created;
}

// 自分の擬似ハンドルでは失敗しない。万一の失敗は 0 で、名前は `<pid>-0` になる。
ProcessInstance current_process_instance() noexcept
{
    FILETIME created{};
    FILETIME exited{};
    FILETIME in_kernel{};
    FILETIME in_user{};
    const bool read =
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &in_kernel, &in_user) != 0;
    return ProcessInstance{static_cast<std::uint32_t>(GetCurrentProcessId()),
                           read ? ticks_of(created) : 0U};
}
} // namespace nenenib::adapters::win32
