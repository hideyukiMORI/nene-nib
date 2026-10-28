#include "ServerChild.hpp"

#include "EventHandle.hpp"
#include "InheritedHandles.hpp"
#include "NativePath.hpp"
#include "OverlappedDrain.hpp"
#include "ServerCommandLine.hpp"
#include "Utf16.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
namespace
{
using application::ServerFailure;

// パイプの中の置き場（両向き）。大きな didOpen は書きの行列が分けて流す。
constexpr DWORD pipe_buffer_bytes = 64 * 1024;
// Job Object で殺したときの終了コード。
constexpr UINT killed_exit_code = 1;

std::wstring widen(std::string_view utf8)
{
    return core::to_utf16(utf8).value_or(std::wstring{});
}

// パイプの名前の通し番号。プロセスで 1 つで、UI スレッドだけが進める。
std::uint64_t next_pipe_sequence() noexcept
{
    static std::uint64_t issued = 0;
    return ++issued;
}

// `\\.\pipe\nenenib-lsp-<pid>-<n>`。FILE_FLAG_FIRST_PIPE_INSTANCE で、同じ名前の他者の instance
// に繋がない。
std::wstring pipe_name()
{
    return L"\\\\.\\pipe\\nenenib-lsp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
           std::to_wstring(next_pipe_sequence());
}

bool runnable(const std::wstring &executable)
{
    if (executable.empty())
    {
        return false;
    }
    const DWORD attributes = GetFileAttributesW(native_path(executable).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

HANDLE server_pipe(const std::wstring &name, DWORD direction)
{
    return CreateNamedPipeW(
        name.c_str(), direction | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
        pipe_buffer_bytes, pipe_buffer_bytes, 0, nullptr);
}

// 子の側の端。継承できる handle で、同期の I/O（子の C の実行時が普通に読み書きする）。
HANDLE inheritable_file(const wchar_t *name, DWORD access)
{
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    return CreateFileW(name, access, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                       0, nullptr);
}

// 子の端を開いた後の繋がりの確認。既に繋がっていれば ERROR_PIPE_CONNECTED（ADR 0126
// の公式資料の節）。
bool connected(HANDLE pipe)
{
    const EventHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.valid())
    {
        return false;
    }
    OVERLAPPED overlapped{};
    overlapped.hEvent = event.get();
    if (ConnectNamedPipe(pipe, &overlapped) != 0)
    {
        return true;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_IO_PENDING)
    {
        drain_overlapped(pipe, overlapped);
    }
    return error == ERROR_PIPE_CONNECTED;
}

bool kill_on_close(HANDLE job)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    return SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                                   sizeof(limits)) != 0;
}

ServerFailure start_failure(DWORD error) noexcept
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
               ? ServerFailure::not_found
               : ServerFailure::cannot_start;
}
} // namespace

ServerChild::ServerChild(std::size_t read_bytes) : read_bytes_(read_bytes) {}

ServerChild::~ServerChild()
{
    end();
}

std::expected<void, ServerFailure> ServerChild::spawn(const application::ServerLaunch &launch)
{
    const std::wstring executable = widen(launch.executable.text());
    if (!runnable(executable))
    {
        return std::unexpected(ServerFailure::not_found);
    }
    KernelHandle child_input;
    KernelHandle child_output;
    if (!connect(child_input, child_output))
    {
        return std::unexpected(ServerFailure::pipe_failed);
    }
    return create(executable, launch, child_input, child_output);
}

HANDLE ServerChild::process() const noexcept
{
    return process_.get();
}

PipeRead &ServerChild::reader() noexcept
{
    return *reader_;
}

PipeWrite &ServerChild::writer() noexcept
{
    return *writer_;
}

void ServerChild::end() noexcept
{
    writer_.reset();
    input_.close();
    reader_.reset();
    output_.close();
    if (process_.valid() &&
        WaitForSingleObject(process_.get(), server_stop_wait_milliseconds) != WAIT_OBJECT_0)
    {
        TerminateJobObject(job_.get(), killed_exit_code);
        WaitForSingleObject(process_.get(), INFINITE);
    }
    process_.close();
    job_.close();
}

// 入力（子が読む）と出力（子が書く）のパイプを作り、子の端を開いて繋ぐ。
bool ServerChild::connect(KernelHandle &child_input, KernelHandle &child_output)
{
    const std::wstring input_name = pipe_name();
    const std::wstring output_name = pipe_name();
    input_.reset(server_pipe(input_name, PIPE_ACCESS_OUTBOUND));
    output_.reset(server_pipe(output_name, PIPE_ACCESS_INBOUND));
    if (!input_.valid() || !output_.valid())
    {
        return false;
    }
    child_input.reset(inheritable_file(input_name.c_str(), GENERIC_READ));
    child_output.reset(inheritable_file(output_name.c_str(), GENERIC_WRITE));
    if (!child_input.valid() || !child_output.valid() || !connected(input_.get()) ||
        !connected(output_.get()))
    {
        return false;
    }
    writer_ = std::make_unique<PipeWrite>(input_.get());
    reader_ = std::make_unique<PipeRead>(output_.get(), read_bytes_);
    return writer_->valid() && reader_->valid();
}

std::expected<void, ServerFailure> ServerChild::create(const std::wstring &executable,
                                                       const application::ServerLaunch &launch,
                                                       const KernelHandle &child_input,
                                                       const KernelHandle &child_output)
{
    const KernelHandle child_error(inheritable_file(L"NUL", GENERIC_WRITE));
    job_.reset(CreateJobObjectW(nullptr, nullptr));
    const InheritedHandles inherited({child_input.get(), child_output.get(), child_error.get()});
    if (!child_error.valid() || !job_.valid() || !kill_on_close(job_.get()) || !inherited.valid())
    {
        return std::unexpected(ServerFailure::cannot_start);
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = child_input.get();
    startup.StartupInfo.hStdOutput = child_output.get();
    startup.StartupInfo.hStdError = child_error.get();
    startup.lpAttributeList = inherited.get();
    std::wstring command = server_command_line(executable, launch.arguments);
    const std::wstring root = widen(launch.root.text());
    PROCESS_INFORMATION information{};
    if (CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                       CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT |
                           EXTENDED_STARTUPINFO_PRESENT,
                       nullptr, root.empty() ? nullptr : root.c_str(), &startup.StartupInfo,
                       &information) == 0)
    {
        return std::unexpected(start_failure(GetLastError()));
    }
    process_.reset(information.hProcess);
    const KernelHandle thread(information.hThread);
    if (AssignProcessToJobObject(job_.get(), process_.get()) == 0)
    {
        TerminateProcess(process_.get(), killed_exit_code);
        return std::unexpected(ServerFailure::cannot_start);
    }
    ResumeThread(thread.get());
    return {};
}
} // namespace nenenib::adapters::win32
