#include "Win32LanguageServer.hpp"

#include <utility>

namespace nenenib::adapters::win32
{
using application::ServerExited;
using application::ServerFailed;
using application::ServerFailure;
using application::ServerMessage;
using application::ServerStarted;

Win32LanguageServer::Win32LanguageServer(std::function<void()> arrived, std::size_t read_bytes)
    : arrived_(std::move(arrived)), read_bytes_(read_bytes)
{
}

Win32LanguageServer::~Win32LanguageServer()
{
    child_.reset();
}

std::expected<void, ServerFailure> Win32LanguageServer::start(application::ServerLaunch launch)
{
    stop();
    auto child = std::make_unique<ServerChild>(read_bytes_);
    const auto spawned = child->spawn(launch);
    if (!spawned.has_value())
    {
        return std::unexpected(spawned.error());
    }
    child_ = std::move(child);
    framer_ = ServerFramer{};
    output_open_ = true;
    writable_ = true;
    post(ServerStarted{});
    issue_read();
    refresh();
    return {};
}

void Win32LanguageServer::send(std::string body)
{
    if (!child_ || !writable_)
    {
        post(ServerFailed{ServerFailure::write_failed});
        return;
    }
    const auto pushed = child_->writer().push(framed_message(body));
    if (!pushed.has_value())
    {
        write_failed();
    }
    refresh();
}

// shutdown は送らない（呼ぶ側の役）。終わらせた子の ServerExited は積まない。
void Win32LanguageServer::stop()
{
    child_.reset();
    exit_code_.reset();
    output_open_ = false;
    writable_ = false;
    refresh();
}

std::vector<application::ServerEvent> Win32LanguageServer::collect()
{
    return std::exchange(events_, {});
}

std::span<const HANDLE> Win32LanguageServer::waitable_handles() const noexcept
{
    return waitable_;
}

void Win32LanguageServer::completed(HANDLE handle)
{
    if (!child_ || handle == nullptr)
    {
        return;
    }
    if (handle == child_->reader().event() && child_->reader().pending())
    {
        read_completed();
    }
    else if (handle == child_->writer().event() && child_->writer().pending())
    {
        write_completed();
    }
    else if (handle == child_->process() && !exit_code_.has_value())
    {
        process_completed();
    }
    finish_if_exited();
    refresh();
}

void Win32LanguageServer::post(application::ServerEvent event)
{
    const bool was_empty = events_.empty();
    events_.push_back(std::move(event));
    if (was_empty && arrived_)
    {
        arrived_();
    }
}

void Win32LanguageServer::issue_read()
{
    const auto issued = child_->reader().issue();
    if (!issued.has_value())
    {
        read_ended(issued.error());
    }
}

// 受けたバイト列は次の ReadFile の前に枠へ写す（buffer は次の issue で上書きされる）。
void Win32LanguageServer::read_completed()
{
    const auto bytes = child_->reader().finish();
    if (!bytes.has_value())
    {
        read_ended(bytes.error());
        return;
    }
    for (auto &body : framer_.feed(bytes.value()))
    {
        post(ServerMessage{std::move(body)});
    }
    const auto failure = framer_.failure();
    if (failure.has_value())
    {
        post(ServerFailed{failure.value()});
        output_open_ = false;
        return;
    }
    issue_read();
}

// 相手が閉じた（ERROR_BROKEN_PIPE / ERROR_HANDLE_EOF）のは終わりで、失敗ではない。
// 未完は何もしない。
void Win32LanguageServer::read_ended(DWORD error)
{
    if (error == ERROR_IO_INCOMPLETE)
    {
        return;
    }
    output_open_ = false;
    if (error != ERROR_BROKEN_PIPE && error != ERROR_HANDLE_EOF)
    {
        post(ServerFailed{ServerFailure::read_failed});
    }
}

void Win32LanguageServer::write_completed()
{
    const auto finished = child_->writer().finish();
    if (!finished.has_value() && finished.error() != ERROR_IO_INCOMPLETE)
    {
        write_failed();
    }
}

// 送れなかった枠と行列の残りは捨て、以後の send は write_failed にする。
void Win32LanguageServer::write_failed()
{
    child_->writer().drain();
    writable_ = false;
    post(ServerFailed{ServerFailure::write_failed});
}

void Win32LanguageServer::process_completed()
{
    DWORD code = 0;
    GetExitCodeProcess(child_->process(), &code);
    exit_code_ = static_cast<std::int32_t>(code);
    writable_ = false;
}

// 子が終わり、出力の読みも終わってから（届いた本文を全部渡してから）ServerExited を積む。
void Win32LanguageServer::finish_if_exited()
{
    if (!child_ || output_open_ || !exit_code_.has_value())
    {
        return;
    }
    const std::int32_t code = exit_code_.value();
    child_.reset();
    exit_code_.reset();
    post(ServerExited{code});
}

void Win32LanguageServer::refresh()
{
    waitable_.clear();
    if (!child_)
    {
        return;
    }
    if (child_->reader().pending())
    {
        waitable_.push_back(child_->reader().event());
    }
    if (child_->writer().pending())
    {
        waitable_.push_back(child_->writer().event());
    }
    if (!exit_code_.has_value())
    {
        waitable_.push_back(child_->process());
    }
}
} // namespace nenenib::adapters::win32
