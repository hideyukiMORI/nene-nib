#include "Win32LanguageServer.hpp"

#include "LspFrameFailure.hpp"
#include "LspFrames.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::adapters::win32
{
using application::LanguageServerEvent;
using application::LanguageServerSlot;
using application::ServerErrorOutput;
using application::ServerExited;
using application::ServerFailed;
using application::ServerFailure;
using application::ServerMessage;
using application::ServerStarted;

namespace
{
// 席ごとに待つ handle の数（出ている読み・出ている標準エラーの読み・出ている書き・起きている子・
// 2026-11-22 の追補）。
constexpr std::size_t handles_per_child = 4;
// MsgWaitForMultipleObjectsEx が待てる数は MAXIMUM_WAIT_OBJECTS - 1（ADR 0160 の決定 2）。
static_assert(application::language_server_slot_limit * handles_per_child +
                      language_server_retired_limit <
                  MAXIMUM_WAIT_OBJECTS,
              "the pump waits on every slot's handles and the retired children together");

// 枠の閉じた失敗を port の失敗へ写す（ADR 0160 の決定 3・6）。見出しの誤りは read_failed。
ServerFailure failure_of(core::LspFrameFailure failure) noexcept
{
    switch (failure)
    {
    case core::LspFrameFailure::malformed_header:
    case core::LspFrameFailure::missing_content_length:
    case core::LspFrameFailure::invalid_content_length:
        return ServerFailure::read_failed;
    case core::LspFrameFailure::body_too_long:
        return ServerFailure::message_too_long;
    case core::LspFrameFailure::header_too_long:
        return ServerFailure::header_too_long;
    }
    std::unreachable();
}

// 席 slot の標準エラーの塊のバイトの和。
std::size_t error_bytes_of(const std::vector<LanguageServerEvent> &events, LanguageServerSlot slot)
{
    std::size_t total = 0;
    for (const LanguageServerEvent &event : events)
    {
        const auto *output = std::get_if<ServerErrorOutput>(&event.event);
        total += event.slot == slot && output != nullptr ? output->bytes.size() : 0;
    }
    return total;
}

// 席 slot の塊の和が上限に収まるまで、古い塊の古いバイトから捨て、空になった塊を外す。
void trim_error_output(std::vector<LanguageServerEvent> &events, LanguageServerSlot slot)
{
    std::size_t total = error_bytes_of(events, slot);
    for (LanguageServerEvent &event : events)
    {
        auto *output = std::get_if<ServerErrorOutput>(&event.event);
        if (total <= language_server_error_queue_bytes)
        {
            break;
        }
        if (event.slot != slot || output == nullptr)
        {
            continue;
        }
        const std::size_t cut =
            std::min(total - language_server_error_queue_bytes, output->bytes.size());
        output->bytes.erase(0, cut);
        total -= cut;
    }
    std::erase_if(events,
                  [slot](const LanguageServerEvent &event)
                  {
                      const auto *output = std::get_if<ServerErrorOutput>(&event.event);
                      return event.slot == slot && output != nullptr && output->bytes.empty();
                  });
}
} // namespace

void queue_error_output(std::vector<LanguageServerEvent> &events, LanguageServerSlot slot,
                        std::string_view bytes)
{
    if (bytes.empty())
    {
        return;
    }
    const auto latest = std::ranges::find_if(events.rbegin(), events.rend(),
                                             [slot](const LanguageServerEvent &event)
                                             { return event.slot == slot; });
    auto *last = latest == events.rend() ? nullptr : std::get_if<ServerErrorOutput>(&latest->event);
    if (last == nullptr)
    {
        events.push_back(LanguageServerEvent{.slot = slot, .event = ServerErrorOutput{}});
        last = &std::get<ServerErrorOutput>(events.back().event);
    }
    last->bytes.append(bytes);
    trim_error_output(events, slot);
}

Win32LanguageServer::Win32LanguageServer(std::function<void()> arrived, std::size_t read_bytes)
    : arrived_(std::move(arrived)), read_bytes_(read_bytes)
{
}

// 待たない。子と退く子の job を閉じると、KILL_ON_JOB_CLOSE が残っている子と子孫を殺す。
Win32LanguageServer::~Win32LanguageServer()
{
    for (auto &child : children_)
    {
        child.reset();
    }
    retired_.clear();
}

std::expected<void, ServerFailure> Win32LanguageServer::start(LanguageServerSlot slot,
                                                              application::ServerLaunch launch)
{
    // 前の子の残りは呼ぶ側が stop で受け取っている（ここで外す分は誰も読まない）。
    static_cast<void>(stop(slot));
    const std::size_t index = application::slot_index(slot);
    auto child = std::make_unique<ServerChild>(read_bytes_);
    const auto spawned = child->spawn(launch);
    if (!spawned.has_value())
    {
        return std::unexpected(spawned.error());
    }
    children_.at(index) = std::move(child);
    pending_.at(index).clear();
    writable_.at(index) = true;
    post(index, ServerStarted{});
    issue_read(index);
    issue_error_read(index);
    refresh();
    return {};
}

void Win32LanguageServer::send(LanguageServerSlot slot, std::string body)
{
    const std::size_t index = application::slot_index(slot);
    const auto &child = children_.at(index);
    if (!child || !writable_.at(index))
    {
        post(index, ServerFailed{ServerFailure::write_failed});
        return;
    }
    const auto pushed = child->writer().push(core::lsp_framed(body));
    if (!pushed.has_value())
    {
        write_failed(index);
    }
    refresh();
}

// shutdown は送らない（呼ぶ側の役）。パイプを閉じた子は退く子の枠へ移し、待たずに戻る（ADR 0160
// の追補）。閉じる前に完了済みの読み（標準エラーと本文）を回収し、その席のたまっていた出来事を
// 列から外して届いた順に返す（古い子の本文を次の子の前に混ぜない・ほかの席の出来事は残す・
// controller は記録にだけ写す・#1950 のレビュー M1）。
std::vector<application::ServerEvent> Win32LanguageServer::stop(LanguageServerSlot slot)
{
    const std::size_t index = application::slot_index(slot);
    auto &child = children_.at(index);
    if (child)
    {
        drain_errors(index);
        drain_output(index);
    }
    std::vector<application::ServerEvent> unread;
    for (const LanguageServerEvent &event : events_)
    {
        if (event.slot == slot)
        {
            unread.push_back(event.event);
        }
    }
    std::erase_if(events_, [slot](const LanguageServerEvent &event) { return event.slot == slot; });
    // 列が空になったら合図の印も戻す（controller は子が居なければ collect を呼ばないので、出した
    // 合図が回収されないまま次の start の合図を止めない・2026-11-22 の追補）。
    signalled_ = signalled_ && !events_.empty();
    if (child)
    {
        child->close_pipes();
        retire(std::move(child));
    }
    writable_.at(index) = false;
    refresh();
    return unread;
}

// 退く子の枠へ移す。上限に達していれば最も古い退く子を先に殺して片付ける（待たない）。
void Win32LanguageServer::retire(std::unique_ptr<ServerChild> child)
{
    if (retired_.size() >= language_server_retired_limit)
    {
        retired_.front()->terminate();
        retired_.erase(retired_.begin());
    }
    retired_.push_back(std::move(child));
}

std::vector<LanguageServerEvent> Win32LanguageServer::collect()
{
    signalled_ = false;
    return std::exchange(events_, {});
}

void Win32LanguageServer::bind(std::function<void()> arrived)
{
    arrived_ = std::move(arrived);
    // 結ぶ前（窓の前・controller の構築の中の start）に溜まった出来事は合図が出ていないので、
    // ここで 1 回知らせる（post は合図が出ていないときしか知らせない）。
    if (!events_.empty() && arrived_)
    {
        signalled_ = true;
        arrived_();
    }
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
    if (!retire_finished(handle))
    {
        children_completed(handle);
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

// 起きている子のうち handle を持つ席の I/O を回収する。
void Win32LanguageServer::children_completed(HANDLE handle)
{
    for (std::size_t index = 0; index < children_.size(); ++index)
    {
        if (children_.at(index))
        {
            child_completed(index, handle);
        }
    }
}

// 席 index の子の handle なら回収する（ほかの席の handle なら何もしない）。
void Win32LanguageServer::child_completed(std::size_t index, HANDLE handle)
{
    auto &child = *children_.at(index);
    if (handle == child.reader().event() && child.reader().pending())
    {
        read_completed(index);
    }
    else if (handle == child.error_reader().event() && child.error_reader().pending())
    {
        error_read_completed(index);
    }
    else if (handle == child.writer().event() && child.writer().pending())
    {
        write_completed(index);
    }
    else if (handle == child.process())
    {
        process_completed(index);
    }
}

// 合図はまだ collect されていない合図が無いときだけ出す（列に合図を出さない標準エラーの塊が
// 残っていても出す・2026-11-22 の追補）。
void Win32LanguageServer::post(std::size_t index, application::ServerEvent event)
{
    events_.push_back(LanguageServerEvent{.slot = application::language_server_slots.at(index),
                                          .event = std::move(event)});
    if (!signalled_ && arrived_)
    {
        signalled_ = true;
        arrived_();
    }
}

void Win32LanguageServer::issue_read(std::size_t index)
{
    const auto issued = children_.at(index)->reader().issue();
    if (!issued.has_value())
    {
        read_ended(index, issued.error());
    }
}

// 受けたバイト列は次の ReadFile の前に枠へ写す（buffer は次の issue で上書きされる）。
void Win32LanguageServer::read_completed(std::size_t index)
{
    const auto bytes = children_.at(index)->reader().finish();
    if (!bytes.has_value())
    {
        read_ended(index, bytes.error());
        return;
    }
    if (deliver(index, bytes.value()))
    {
        issue_read(index);
    }
}

// 繋いだバイト列を core の lsp_frames で切り、揃った本文を積んで consumed だけ捨てる。枠が
// 壊れたら知らせて false（流れの区切りを失ったので以後は読まない・同じ読みの中で壊れた枠の前に
// あった本文も積まない）。
bool Win32LanguageServer::deliver(std::size_t index, std::string_view bytes)
{
    auto &pending = pending_.at(index);
    pending.append(bytes);
    const auto frames = core::lsp_frames(pending);
    if (!frames.has_value())
    {
        pending.clear();
        post(index, ServerFailed{failure_of(frames.error())});
        return false;
    }
    for (const std::string_view body : frames.value().bodies)
    {
        post(index, ServerMessage{std::string(body)});
    }
    pending.erase(0, frames.value().consumed);
    return true;
}

// 標準エラーの読みを 1 本出す（2026-11-22 の追補）。出せなければ・終わりなら以後は読まない
// （標準エラーを失ってもサーバーは止めない）。
void Win32LanguageServer::issue_error_read(std::size_t index)
{
    static_cast<void>(children_.at(index)->error_reader().issue());
}

void Win32LanguageServer::error_read_completed(std::size_t index)
{
    const auto bytes = children_.at(index)->error_reader().finish();
    if (!bytes.has_value())
    {
        return;
    }
    post_error_output(index, bytes.value());
    issue_error_read(index);
}

// 合図を出さずに積む（2026-11-22 の追補）。列が空でも合図は出さない（次の本文か子の終わりと
// 一緒に集められる）。
void Win32LanguageServer::post_error_output(std::size_t index, std::string_view bytes)
{
    queue_error_output(events_, application::language_server_slots.at(index), bytes);
}

// 出ている標準エラーの読みが完了していれば回収して次を出す（子の終わりで、終わる前に書かれた分を
// 渡す）。パイプが空なら読みは未完のまま止まる。
void Win32LanguageServer::drain_errors(std::size_t index)
{
    auto &reader = children_.at(index)->error_reader();
    while (reader.pending())
    {
        const auto bytes = reader.finish();
        if (!bytes.has_value())
        {
            return;
        }
        post_error_output(index, bytes.value());
        issue_error_read(index);
    }
}

// 相手が閉じた（ERROR_BROKEN_PIPE / ERROR_HANDLE_EOF）のは終わりで、失敗ではない。
// 未完は何もしない。
void Win32LanguageServer::read_ended(std::size_t index, DWORD error)
{
    if (error != ERROR_IO_INCOMPLETE && error != ERROR_BROKEN_PIPE && error != ERROR_HANDLE_EOF)
    {
        post(index, ServerFailed{ServerFailure::read_failed});
    }
}

void Win32LanguageServer::write_completed(std::size_t index)
{
    const auto finished = children_.at(index)->writer().finish();
    if (!finished.has_value() && finished.error() != ERROR_IO_INCOMPLETE)
    {
        write_failed(index);
    }
}

// 送れなかった枠と行列の残りは捨て、以後のその席の send は write_failed にする。
void Win32LanguageServer::write_failed(std::size_t index)
{
    children_.at(index)->writer().drain();
    writable_.at(index) = false;
    post(index, ServerFailed{ServerFailure::write_failed});
}

// 子が終わったら子孫を Job Object で片付け、出力の EOF を待たずに ServerExited へ進む（ADR 0160
// の追補）。終わる前に書かれた本文はパイプに残っているので、待たずに読める分を先に渡す。
void Win32LanguageServer::process_completed(std::size_t index)
{
    auto &child = children_.at(index);
    child->terminate();
    drain_errors(index);
    drain_output(index);
    DWORD code = 0;
    GetExitCodeProcess(child->process(), &code);
    child.reset();
    writable_.at(index) = false;
    post(index, ServerExited{static_cast<std::int32_t>(code)});
}

// 出ている読みが完了していれば回収して次を出す。パイプが空なら読みは未完のまま止まる。
void Win32LanguageServer::drain_output(std::size_t index)
{
    auto &reader = children_.at(index)->reader();
    while (reader.pending())
    {
        const auto bytes = reader.finish();
        if (!bytes.has_value())
        {
            read_ended(index, bytes.error());
            return;
        }
        if (!deliver(index, bytes.value()))
        {
            return;
        }
        issue_read(index);
    }
}

void Win32LanguageServer::refresh()
{
    waitable_.clear();
    for (const auto &child : children_)
    {
        if (!child)
        {
            continue;
        }
        if (child->reader().pending())
        {
            waitable_.push_back(child->reader().event());
        }
        if (child->error_reader().pending())
        {
            waitable_.push_back(child->error_reader().event());
        }
        if (child->writer().pending())
        {
            waitable_.push_back(child->writer().event());
        }
        waitable_.push_back(child->process());
    }
    for (const auto &retired : retired_)
    {
        waitable_.push_back(retired->process());
    }
}
} // namespace nenenib::adapters::win32
