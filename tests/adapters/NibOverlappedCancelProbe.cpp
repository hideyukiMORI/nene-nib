#include "OverlappedNativeSignals.hpp"
#include "OverlappedProbeChecks.hpp"
#include "ProbeNativeReadTransport.hpp"
#include "ProbePipeFixture.hpp"
#include "ProbeReadOwner.hpp"
#include "ProbeScriptReadTransport.hpp"
#include <array>
#include <cstdio>
#include <expected>
#include <memory>
#include <optional>
#include <semaphore>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
using namespace nenenib::adapters::win32;
using nenenib::tests::OverlappedNativeSignals;
using nenenib::tests::OverlappedProbeChecks;
using Script = ProbeScriptReadTransport;
using Reply = ProbeIoReply;
using Kind = ProbeReplyKind;
using App = ProbeReadApplication;
using Cancel = ProbeCancelKind;
using Signal = std::binary_semaphore;
constexpr wchar_t script_path[] = L"overlapped-probe-input.bin";

[[nodiscard]] std::unique_ptr<FileHandle> script_file()
{
    return std::make_unique<FileHandle>(CreateFileW(script_path, GENERIC_READ, FILE_SHARE_READ,
                                                    nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                                                    nullptr));
}

[[nodiscard]] std::expected<ProbeReadOwner, ProbeReadFailure>
owner(std::shared_ptr<ProbeReadTransport> io)
{
    return ProbeReadOwner::create(script_file(), 3, std::move(io));
}

void expect_terminal(OverlappedProbeChecks &checks, const ProbeReadTicket &ticket, Reply expected,
                     App app)
{
    const auto result = ticket.collect();
    checks.expect(result.has_value() && result.value().os().kind() == expected.kind() &&
                      result.value().os().error() == expected.error() &&
                      result.value().application() == app,
                  "OS terminal and application adoption are recorded separately");
    if (result)
    {
        checks.expect(result.value().bytes() == (app == App::delivered ? "xyz" : ""),
                      "only delivered bytes are published");
    }
    const auto copy = ticket;
    checks.expect(copy.collect().error() == ProbeReadFailure::consumed,
                  "collect is shared and single-use");
    checks.expect(ticket.cancel() == Cancel::too_late,
                  "cancel after publish leaves result unchanged");
    checks.expect(!ticket.storage_alive(),
                  "idle worker/control/result do not pin operation storage");
}

void verify_basic(OverlappedProbeChecks &checks, Reply issue, std::vector<Reply> queries)
{
    Signal finished{0};
    const auto io = std::make_shared<Script>(
        issue, std::move(queries), std::vector<std::expected<void, std::uint32_t>>{}, "xyz");
    io->set_hook(ProbeReadHook::finished, [&finished] { finished.release(); });
    auto request = owner(io);
    if (!request)
    {
        checks.expect(false, "create basic owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    const auto ticket_copy = ticket;
    checks.expect(ticket.identity().same(ticket_copy.identity()),
                  "copied ticket keeps the same request identity");
    checks.expect(ticket.collect().error() == ProbeReadFailure::not_ready,
                  "pre-run collect is not ready");
    Win32Worker worker;
    checks.expect(request.value().run(worker).has_value(), "first run posts one job");
    checks.expect(request.value().run(worker).error() == ProbeReadFailure::consumed,
                  "duplicate run refused");
    finished.acquire();
    const bool failed = issue.kind() == Kind::failed;
    expect_terminal(checks, ticket, failed ? issue : Reply(Kind::complete, 0, 3),
                    failed ? App::failed : App::delivered);
    checks.expect(io->issues() == 1 && io->cancels() == 0,
                  "one issue and no cancel for basic path");
}

void verify_queued(OverlappedProbeChecks &checks)
{
    Signal finished{0};
    const auto io =
        std::make_shared<Script>(Reply(Kind::immediate), std::vector<Reply>{},
                                 std::vector<std::expected<void, std::uint32_t>>{}, "xyz");
    io->set_hook(ProbeReadHook::finished, [&finished] { finished.release(); });
    auto original = owner(io);
    if (!original)
    {
        checks.expect(false, "queued owner");
        return;
    }
    const auto ticket = original.value().ticket().value();
    const auto copy = ticket;
    checks.expect(ticket.cancel() == Cancel::accepted && copy.cancel() == Cancel::already_requested,
                  "queued cancel is sticky across tickets");
    auto moved = std::move(original.value());
    Win32Worker worker;
    checks.expect(original.value().run(worker).error() == ProbeReadFailure::consumed,
                  "moved-from owner cannot run");
    checks.expect(moved.run(worker).has_value(), "moved owner posts cancelled job");
    finished.acquire();
    expect_terminal(checks, ticket, Reply(Kind::not_issued), App::cancelled);
    checks.expect(io->issues() == 0 && io->queries() == 0 && io->cancels() == 0,
                  "queued cancellation invokes no OS operation");
}

void verify_cancel_case(OverlappedProbeChecks &checks, ProbeReadHook boundary, Reply terminal,
                        std::vector<std::expected<void, std::uint32_t>> cancellations)
{
    const auto expected_trace = cancellations;
    Signal finished{0};
    const auto io = std::make_shared<Script>(
        Reply(Kind::pending, ERROR_IO_PENDING),
        std::vector<Reply>{Reply(Kind::incomplete, ERROR_IO_INCOMPLETE), terminal},
        std::move(cancellations), "xyz");
    auto request = owner(io);
    if (!request)
    {
        checks.expect(false, "cancel owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    io->set_hook(boundary,
                 [&checks, &ticket]
                 {
                     checks.expect(ticket.storage_alive(), "cancel boundary still owns storage");
                     checks.expect(ticket.cancel() == Cancel::accepted,
                                   "first cancel accepted before cutoff");
                     checks.expect(ticket.cancel() == Cancel::already_requested,
                                   "second cancel performs no extra API call");
                 });
    io->set_hook(ProbeReadHook::finished, [&finished] { finished.release(); });
    Win32Worker worker;
    checks.expect(request.value().run(worker).has_value(), "post cancellation case");
    finished.acquire();
    expect_terminal(checks, ticket, terminal, App::cancelled);
    checks.expect(io->queries() == 2, "INCOMPLETE is followed by terminal blocking query");
    const auto expected = boundary == ProbeReadHook::issue ? std::size_t{2} : std::size_t{1};
    checks.expect(io->cancels() == expected, "only issuing return rearms exact cancellation");
    const auto trace = io->cancel_trace();
    checks.expect(trace.size() == expected && trace == expected_trace,
                  "cancel diagnostics are separate immutable call observations");
}

void verify_incomplete(OverlappedProbeChecks &checks)
{
    Signal finished{0};
    const auto io =
        std::make_shared<Script>(Reply(Kind::pending, ERROR_IO_PENDING),
                                 std::vector<Reply>{Reply(Kind::incomplete, ERROR_IO_INCOMPLETE),
                                                    Reply(Kind::complete, 0, 3)},
                                 std::vector<std::expected<void, std::uint32_t>>{}, "xyz");
    auto request = owner(io);
    if (!request)
    {
        checks.expect(false, "incomplete owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    io->set_hook(
        ProbeReadHook::query, [&checks, &ticket]
        { checks.expect(ticket.storage_alive(), "both completion queries keep storage alive"); });
    io->set_hook(ProbeReadHook::finished, [&finished] { finished.release(); });
    Win32Worker worker;
    checks.expect(request.value().run(worker).has_value(), "incomplete request posts");
    finished.acquire();
    expect_terminal(checks, ticket, Reply(Kind::complete, 0, 3), App::delivered);
    checks.expect(io->query_waits() ==
                      std::vector<ProbeQueryWait>{ProbeQueryWait::poll, ProbeQueryWait::wait},
                  "pending query switches from poll to wait");
}

void verify_cancel_pin(OverlappedProbeChecks &checks)
{
    Signal pending{0}, inside_cancel{0}, release_cancel{0}, finished{0};
    const auto io = std::make_shared<Script>(
        Reply(Kind::pending, ERROR_IO_PENDING), std::vector<Reply>{Reply(Kind::complete, 0, 3)},
        std::vector<std::expected<void, std::uint32_t>>{{}}, "xyz");
    auto request = owner(io);
    if (!request)
    {
        checks.expect(false, "pin race owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    std::optional<ProbeReadResult> retained;
    io->set_hook(ProbeReadHook::pending,
                 [&pending, &inside_cancel]
                 {
                     pending.release();
                     inside_cancel.acquire();
                 });
    io->set_hook(ProbeReadHook::cancel,
                 [&inside_cancel, &release_cancel]
                 {
                     inside_cancel.release();
                     release_cancel.acquire();
                 });
    io->set_hook(
        ProbeReadHook::finished,
        [&]
        {
            checks.expect(ticket.storage_alive(), "cancel API local pin survives worker release");
            auto result = ticket.collect();
            checks.expect(
                result.has_value() && result.value().application() == App::cancelled &&
                    result.value().os().kind() == Kind::complete,
                "publish cutoff rejects OS-success bytes while cancel call is still running");
            if (result)
            {
                retained = std::move(result).value();
            }
            release_cancel.release();
            finished.release();
        });
    Win32Worker worker;
    checks.expect(request.value().run(worker).has_value(), "post pin race request");
    pending.acquire();
    checks.expect(ticket.cancel() == Cancel::accepted, "control thread's direct cancel returns");
    finished.acquire();
    checks.expect(!ticket.storage_alive() && retained.has_value() &&
                      retained.value().bytes().empty(),
                  "last cancel pin disappears while immutable result survives");
    checks.expect(io->cancel_trace().size() == 1, "late API return appears in trace only");
}

void verify_unrun_release(OverlappedProbeChecks &checks)
{
    const auto io =
        std::make_shared<Script>(Reply(Kind::immediate), std::vector<Reply>{},
                                 std::vector<std::expected<void, std::uint32_t>>{}, "xyz");
    auto request = owner(io);
    if (!request)
    {
        checks.expect(false, "unrun owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    {
        auto moved = std::move(request.value());
        checks.expect(ticket.storage_alive(), "unrun owner holds storage");
    }
    checks.expect(!ticket.storage_alive() && io->issues() == 0,
                  "discarding unrun owner releases resources without ticket side effects");
}

void install_native_cleanup(ProbeReadTransport &io, ProbePipeFixture &fixture)
{
    io.set_hook(ProbeReadHook::cancel_return,
                [&io, &fixture]
                {
                    const auto trace = io.cancel_trace();
                    if (!trace.empty() && !trace.back().has_value())
                    {
                        fixture.close_writer();
                    }
                });
}

[[nodiscard]] bool await_pending(OverlappedProbeChecks &checks, OverlappedNativeSignals &signals,
                                 ProbeReadTransport &io, const ProbeReadTicket &ticket)
{
    signals.wait_boundary();
    const auto reply = io.issue_reply();
    if (reply.has_value() && reply.value().kind() == Kind::pending)
    {
        return true;
    }
    checks.expect(false, "native pending prerequisite failed explicitly without waiting forever");
    signals.wait_finished();
    checks.expect(ticket.collect().has_value(), "nonpending native failure is still recovered");
    return false;
}

void verify_distinct_identity(OverlappedProbeChecks &checks)
{
    Signal finished{0};
    const auto io = std::make_shared<Script>(
        Reply(Kind::immediate), std::vector<Reply>{Reply(Kind::complete, 0, 3)},
        std::vector<std::expected<void, std::uint32_t>>{}, "xyz");
    io->set_hook(ProbeReadHook::finished, [&finished] { finished.release(); });
    auto first = owner(io);
    if (!first)
    {
        checks.expect(false, "first identity owner");
        return;
    }
    const auto old_ticket = first.value().ticket().value();
    Win32Worker worker;
    checks.expect(first.value().run(worker).has_value(), "identity request runs");
    finished.acquire();
    const auto old_result = old_ticket.collect();
    const auto other_io =
        std::make_shared<Script>(Reply(Kind::immediate), std::vector<Reply>{},
                                 std::vector<std::expected<void, std::uint32_t>>{}, "xyz");
    auto second = owner(other_io);
    if (!second)
    {
        checks.expect(false, "second identity owner");
        return;
    }
    const auto new_ticket = second.value().ticket().value();
    checks.expect(old_result.has_value() &&
                      !old_result.value().identity().same(new_ticket.identity()),
                  "a different request identity differs from the live old result");
}

void verify_native_pending(OverlappedProbeChecks &checks)
{
    OverlappedNativeSignals signals;
    auto fixture = ProbePipeFixture::create();
    if (!fixture)
    {
        checks.expect(false, "create native pending pipe");
        return;
    }
    const auto io = std::make_shared<ProbeNativeReadTransport>();
    signals.bind(*io);
    install_native_cleanup(*io, fixture.value());
    auto request = ProbeReadOwner::create(fixture.value().take_reader(), 3, io);
    if (!request)
    {
        checks.expect(false, "create native pending owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    Win32Worker worker;
    checks.expect(request.value().run(worker).has_value(), "post native pending read");
    if (!await_pending(checks, signals, *io, ticket))
    {
        return;
    }
    checks.expect(true, "native ReadFile actually returned ERROR_IO_PENDING");
    checks.expect(ticket.cancel() == Cancel::accepted,
                  "other thread requests exact native CancelIoEx");
    const auto trace = io->cancel_trace();
    const bool cancelled = trace.size() == 1 && trace.front().has_value();
    checks.expect(cancelled, "native exact cancel request returned success");
    if (!cancelled)
    {
        fixture.value().close_writer();
    }
    signals.wait_finished();
    const auto result = ticket.collect();
    checks.expect(result.has_value() && result.value().os().error() == ERROR_OPERATION_ABORTED &&
                      result.value().application() == App::cancelled,
                  "native cancelled operation is drained as ERROR_OPERATION_ABORTED");
    checks.expect(!ticket.storage_alive(),
                  "native storage released after drain with worker still idle");
    const auto issue = io->issue_reply();
    std::printf("native pending: issue=%u completion=%u cancel_calls=%zu\n",
                issue.has_value() ? static_cast<unsigned>(issue.value().error()) : 0U,
                result.has_value() ? static_cast<unsigned>(result.value().os().error()) : 0U,
                io->cancels());
}

void verify_native_immediate(OverlappedProbeChecks &checks)
{
    Signal finished{0};
    auto fixture = ProbePipeFixture::create();
    if (!fixture)
    {
        checks.expect(false, "create native immediate pipe");
        return;
    }
    const bool written = fixture.value().write("xyz");
    checks.expect(written, "write payload before overlapped read");
    if (!written)
    {
        return;
    }
    const auto io = std::make_shared<ProbeNativeReadTransport>();
    io->set_hook(ProbeReadHook::finished, [&finished] { finished.release(); });
    auto request = ProbeReadOwner::create(fixture.value().take_reader(), 3, io);
    if (!request)
    {
        checks.expect(false, "create native immediate owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    Win32Worker worker;
    checks.expect(request.value().run(worker).has_value(), "post native pre-payload read");
    finished.acquire();
    const auto issue = io->issue_reply();
    checks.expect(issue.has_value() && issue.value().kind() == Kind::immediate,
                  "native ReadFile immediate TRUE is actually observed, without retry");
    const auto result = ticket.collect();
    checks.expect(result.has_value() && result.value().bytes() == "xyz" &&
                      result.value().os().bytes() == 3 &&
                      result.value().application() == App::delivered,
                  "GetOverlappedResult FALSE reports exact immediate bytes and content");
    checks.expect(io->query_waits() == std::vector<ProbeQueryWait>{ProbeQueryWait::poll},
                  "immediate TRUE queries completion without waiting");
    checks.expect(!ticket.storage_alive(), "immediate native storage is released");
    std::printf("native immediate: ReadFile_TRUE=%u bytes=%zu queries=%zu\n",
                issue.has_value() && issue.value().kind() == Kind::immediate ? 1U : 0U,
                result.has_value() ? result.value().os().bytes() : 0, io->queries());
}

void verify_worker_stop(OverlappedProbeChecks &checks)
{
    OverlappedNativeSignals signals;
    auto fixture = ProbePipeFixture::create();
    if (!fixture)
    {
        checks.expect(false, "create worker stop pipe");
        return;
    }
    const auto io = std::make_shared<ProbeNativeReadTransport>();
    signals.bind(*io);
    install_native_cleanup(*io, fixture.value());
    auto request = ProbeReadOwner::create(fixture.value().take_reader(), 3, io);
    if (!request)
    {
        checks.expect(false, "create stop owner");
        return;
    }
    const auto ticket = request.value().ticket().value();
    auto worker = std::make_unique<Win32Worker>();
    checks.expect(request.value().run(*worker).has_value(), "start worker stop read");
    if (!await_pending(checks, signals, *io, ticket))
    {
        return;
    }
    worker.reset();
    signals.wait_finished();
    const auto result = ticket.collect();
    checks.expect(result.has_value() && result.value().os().error() == ERROR_OPERATION_ABORTED &&
                      result.value().application() == App::cancelled,
                  "stop callback uses direct cancel before worker joins");
    checks.expect(io->cancels() == 1 && !ticket.storage_alive(),
                  "worker stop drains then releases operation");
}

void verify_script_cases(OverlappedProbeChecks &checks)
{
    verify_basic(checks, Reply(Kind::immediate), {Reply(Kind::complete, 0, 3)});
    verify_basic(checks, Reply(Kind::failed, ERROR_READ_FAULT), {});
    verify_queued(checks);
    verify_cancel_case(checks, ProbeReadHook::issue, Reply(Kind::failed, ERROR_OPERATION_ABORTED),
                       {std::unexpected(ERROR_NOT_FOUND), {}});
    verify_cancel_case(checks, ProbeReadHook::pending, Reply(Kind::failed, ERROR_OPERATION_ABORTED),
                       {{}});
    verify_cancel_case(checks, ProbeReadHook::pending, Reply(Kind::complete, 0, 3), {{}});
    verify_cancel_case(checks, ProbeReadHook::pending, Reply(Kind::failed, ERROR_READ_FAULT), {{}});
    verify_cancel_case(checks, ProbeReadHook::pending, Reply(Kind::failed, ERROR_READ_FAULT),
                       {std::unexpected(ERROR_ACCESS_DENIED)});
    verify_cancel_case(checks, ProbeReadHook::before_publish, Reply(Kind::complete, 0, 3), {{}});
    verify_incomplete(checks);
    verify_cancel_pin(checks);
    verify_unrun_release(checks);
    verify_distinct_identity(checks);
}
} // namespace
int main()
{
    OverlappedProbeChecks checks;
    {
        const FileHandle fixture(CreateFileW(script_path, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                             FILE_ATTRIBUTE_NORMAL, nullptr));
        checks.expect(fixture.valid(), "fresh script fixture required");
        if (!fixture.valid())
        {
            return checks.finish();
        }
        DWORD done = 0;
        checks.expect(WriteFile(fixture.get(), "xyz", 3, &done, nullptr) != 0 && done == 3,
                      "small script fixture bytes");
    }
    verify_script_cases(checks);
    verify_native_pending(checks);
    verify_native_immediate(checks);
    verify_worker_stop(checks);
    checks.expect(DeleteFileW(script_path) != 0,
                  "delete only own script fixture after all operations drain");
    return checks.finish();
}
