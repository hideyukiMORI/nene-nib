#include "Win32LanguageServer.hpp"

#include <algorithm>
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

// 待たない。子と退く子の job を閉じると、KILL_ON_JOB_CLOSE が残っている子と子孫を殺す。
Win32LanguageServer::~Win32LanguageServer()
{
    child_.reset();
    retired_.clear();
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

// shutdown は送らない（呼ぶ側の役）。パイプを閉じた子は退く子の枠へ移し、待たずに戻る（ADR 0160
// の追補）。たまっていた出来事は捨てる（古い子の本文を次の子の前に混ぜない）。
void Win32LanguageServer::stop()
{
    events_.clear();
    if (child_)
    {
        child_->close_pipes();
        retired_.push_back(std::move(child_));
    }
    writable_ = false;
    refresh();
}

std::vector<application::ServerEvent> Win32LanguageServer::collect()
{
    return std::exchange(events_, {});
}

void Win32LanguageServer::bind(std::function<void()> arrived)
{
    arrived_ = std::move(arrived);
}

std::span<const HANDLE> Win32LanguageServer::waitable_handles() const noexcept
{
    return waitable_;
}

void Win32LanguageServer::completed(HANDLE handle)
{
    if (handle == nullptr)
    {
        return;
    }
    if (!retire_finished(handle) && child_)
    {
        child_completed(handle);
    }
    refresh();
}

void Win32LanguageServer::expire_retired()
{
    for (const auto &retired : retired_)
    {
        retired->terminate();
    }
    retired_.clear();
    refresh();
}

// 終わった退く子なら片付けて true（job を閉じると子孫も残らない）。
bool Win32LanguageServer::retire_finished(HANDLE handle)
{
    const auto found = std::ranges::find_if(retired_, [handle](const auto &retired)
                                            { return retired->process() == handle; });
    if (found == retired_.end())
    {
        return false;
    }
    retired_.erase(found);
    return true;
}

void Win32LanguageServer::child_completed(HANDLE handle)
{
    if (handle == child_->reader().event() && child_->reader().pending())
    {
        read_completed();
    }
    else if (handle == child_->writer().event() && child_->writer().pending())
    {
        write_completed();
    }
    else if (handle == child_->process())
    {
        process_completed();
    }
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
    if (deliver(bytes.value()))
    {
        issue_read();
    }
}

// 切り出せた本文を積む。枠が壊れたら知らせて false（以後は読まない）。
bool Win32LanguageServer::deliver(std::string_view bytes)
{
    for (auto &body : framer_.feed(bytes))
    {
        post(ServerMessage{std::move(body)});
    }
    const auto failure = framer_.failure();
    if (failure.has_value())
    {
        post(ServerFailed{failure.value()});
        return false;
    }
    return true;
}

// 相手が閉じた（ERROR_BROKEN_PIPE / ERROR_HANDLE_EOF）のは終わりで、失敗ではない。
// 未完は何もしない。
void Win32LanguageServer::read_ended(DWORD error)
{
    if (error != ERROR_IO_INCOMPLETE && error != ERROR_BROKEN_PIPE && error != ERROR_HANDLE_EOF)
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

// 子が終わったら子孫を Job Object で片付け、出力の EOF を待たずに ServerExited へ進む（ADR 0160
// の追補）。終わる前に書かれた本文はパイプに残っているので、待たずに読める分を先に渡す。
void Win32LanguageServer::process_completed()
{
    child_->terminate();
    drain_output();
    DWORD code = 0;
    GetExitCodeProcess(child_->process(), &code);
    child_.reset();
    writable_ = false;
    post(ServerExited{static_cast<std::int32_t>(code)});
}

// 出ている読みが完了していれば回収して次を出す。パイプが空なら読みは未完のまま止まる。
void Win32LanguageServer::drain_output()
{
    while (child_->reader().pending())
    {
        const auto bytes = child_->reader().finish();
        if (!bytes.has_value())
        {
            read_ended(bytes.error());
            return;
        }
        if (!deliver(bytes.value()))
        {
            return;
        }
        issue_read();
    }
}

void Win32LanguageServer::refresh()
{
    waitable_.clear();
    if (child_)
    {
        if (child_->reader().pending())
        {
            waitable_.push_back(child_->reader().event());
        }
        if (child_->writer().pending())
        {
            waitable_.push_back(child_->writer().event());
        }
        waitable_.push_back(child_->process());
    }
    for (const auto &retired : retired_)
    {
        waitable_.push_back(retired->process());
    }
}
} // namespace nenenib::adapters::win32
