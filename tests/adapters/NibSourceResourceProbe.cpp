#include "FileHandle.hpp"
#include "ProbeSourceTask.hpp"
#include "SourceResourceChecks.hpp"
#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <windows.h>

namespace
{
using namespace nenenib::adapters::win32;
using nenenib::tests::SourceResourceChecks;
using Failure = ProbeSourceFailure;
constexpr wchar_t folder[] = L"source-resource-probe";
constexpr wchar_t input_path[] = L"source-resource-probe/input.bin";
constexpr wchar_t spool_path[] = L"source-resource-probe/spool.bin";
constexpr wchar_t second_spool_path[] = L"source-resource-probe/second-spool.bin";

[[nodiscard]] bool absent(const wchar_t *path)
{
    const auto attributes = GetFileAttributesW(path);
    const auto error = GetLastError();
    return attributes == INVALID_FILE_ATTRIBUTES &&
           (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND);
}

[[nodiscard]] bool set_input(std::string_view bytes)
{
    const FileHandle file(CreateFileW(input_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid())
    {
        return false;
    }
    DWORD written = 0;
    return WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written,
                     nullptr) != 0 &&
           written == bytes.size();
}

[[nodiscard]] bool source_matches(std::string_view expected, const wchar_t *path = input_path)
{
    const FileHandle file(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid())
    {
        return false;
    }
    std::array<char, 64> bytes{};
    DWORD read = 0;
    return ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) !=
               0 &&
           std::string_view(bytes.data(), read) == expected;
}

[[nodiscard]] std::expected<ProbeSealedSource, Failure>
make_source(SourceResourceChecks &checks, std::string_view bytes, std::size_t budget,
            std::shared_ptr<ProbeSourceIo> io)
{
    checks.expect(set_input(bytes), "write small input fixture");
    auto task = ProbeSourceTask::start(input_path, spool_path, std::move(io));
    checks.expect(task.has_value(), "start holds source and private spool");
    if (!task)
    {
        return std::unexpected(task.error());
    }
    if (!bytes.empty())
    {
        checks.expect(task.value().take().error() == Failure::not_ready,
                      "unfinished task cannot publish a sealed resource");
    }
    for (std::size_t at = 0; at < bytes.size(); at += budget)
    {
        const auto step = task.value().step(budget);
        checks.expect(step.has_value(), "bounded copy step succeeds");
        if (!step)
        {
            return std::unexpected(step.error());
        }
    }
    const auto ready = task.value().step(budget);
    checks.expect(ready.has_value() && ready.value() == ProbeSourcePhase::ready,
                  "step after completion reports ready without additional I/O");
    auto sealed = task.value().take();
    checks.expect(sealed.has_value(), "completed task can publish exactly once");
    checks.expect(task.value().take().error() == Failure::unavailable,
                  "second take cannot republish the spool");
    checks.expect(task.value().step(budget).error() == Failure::unavailable,
                  "handed-off task cannot run another copy step");
    task.value().cancel();
    return sealed;
}

void verify_raw_bytes(SourceResourceChecks &checks)
{
    const std::array<std::string, 2> fixtures{"", std::string("\xEF\xBB\xBF\0\xFF\r\n", 7)};
    for (const auto &bytes : fixtures)
    {
        auto sealed = make_source(checks, bytes, 2, std::make_shared<ProbeSourceIo>());
        if (!sealed)
        {
            continue;
        }
        checks.expect(sealed.value().length() == bytes.size(), "declared length survives sealing");
        auto read = sealed.value().request(0, bytes.size());
        if (!read)
        {
            checks.expect(false, "full small range accepted");
            continue;
        }
        const auto result = read.value().run();
        checks.expect(result.has_value() && result.value().bytes() == bytes,
                      "BOM/NUL/invalid UTF8 are opaque bytes, not rejected text");
        sealed.value().release();
        checks.expect(absent(spool_path), "finished result does not pin the deleted spool");
        checks.expect(result.has_value() && result.value().bytes() == bytes,
                      "result bytes survive spool deletion");
    }
}

void verify_fault_case(SourceResourceChecks &checks, ProbeIoFault fault, bool writing, bool partial)
{
    const auto io = std::make_shared<ProbeSourceIo>();
    checks.expect(set_input("abcdef"), "prepare fault fixture");
    auto task = ProbeSourceTask::start(input_path, spool_path, io);
    if (!task)
    {
        checks.expect(false, "start fault task");
        return;
    }
    if (partial)
    {
        checks.expect(task.value().step(2).has_value(), "copy prefix before injected fault");
    }
    if (writing)
    {
        io->script_writes({fault});
    }
    if (!writing)
    {
        io->script_reads({fault});
    }
    const auto result = task.value().step(4);
    checks.expect(!result &&
                      result.error() == (writing ? Failure::unwritable : Failure::unreadable),
                  "injected copy failure is not a completed resource");
    checks.expect(!task.value().take(), "failed task cannot publish a resource");
    task.value().cancel();
    checks.expect(task.value().step(2).error() == Failure::unavailable,
                  "cancel after failure does not change the closed task state");
    checks.expect(absent(spool_path), "copy failure closes and deletes the private spool");
    checks.expect(source_matches("abcdef"), "copy failure leaves original bytes unchanged");
}

void verify_faults(SourceResourceChecks &checks, ProbeIoFault fault, bool writing)
{
    verify_fault_case(checks, fault, writing, false);
    verify_fault_case(checks, fault, writing, true);
}

void verify_short_read(SourceResourceChecks &checks)
{
    const auto io = std::make_shared<ProbeSourceIo>();
    io->script_reads({ProbeIoFault::short_io});
    auto sealed = make_source(checks, "abcdef", 6, io);
    if (!sealed)
    {
        return;
    }
    auto read = sealed.value().request(0, 6);
    if (!read)
    {
        checks.expect(false, "short read resource accepts range");
        return;
    }
    const auto result = read.value().run();
    checks.expect(result.has_value() && result.value().bytes() == "abcdef",
                  "shared CompleteFileRead continues a short successful copy read");
    sealed.value().release();
    checks.expect(absent(spool_path), "short read resource is deleted after release");
}

void verify_cancel(SourceResourceChecks &checks)
{
    for (const std::size_t copied : {std::size_t{0}, std::size_t{2}, std::size_t{6}})
    {
        checks.expect(set_input("abcdef"), "prepare cancel fixture");
        auto task =
            ProbeSourceTask::start(input_path, spool_path, std::make_shared<ProbeSourceIo>());
        if (!task)
        {
            checks.expect(false, "start cancel task");
            continue;
        }
        if (copied != 0)
        {
            checks.expect(task.value().step(copied).has_value(), "step before cancel");
        }
        task.value().cancel();
        task.value().cancel();
        checks.expect(task.value().step(2).error() == Failure::cancelled,
                      "cancel prevents subsequent I/O steps");
        checks.expect(task.value().take().error() == Failure::cancelled,
                      "cancel before handoff prevents sealing even after final copy");
        checks.expect(absent(spool_path), "cancel closes unpublished spool");
    }
}

void verify_release_pin(SourceResourceChecks &checks)
{
    auto sealed = make_source(checks, "abcdef", 6, std::make_shared<ProbeSourceIo>());
    if (!sealed)
    {
        return;
    }
    auto read = sealed.value().request(1, 3);
    if (!read)
    {
        checks.expect(false, "accept pinned range");
        return;
    }
    sealed.value().release();
    sealed.value().release();
    checks.expect(sealed.value().request(0, 1).error() == Failure::unavailable,
                  "release is idempotent and closes new acceptance");
    checks.expect(!absent(spool_path), "accepted request pins spool after owner release");
    const auto result = read.value().run();
    checks.expect(result.has_value() && result.value().bytes() == "bcd" &&
                      result.value().offset() == 1 && result.value().id().same(sealed.value().id()),
                  "accepted range finishes with its resource identity and offset");
    checks.expect(read.value().run().error() == Failure::consumed, "range runs only once");
    checks.expect(absent(spool_path), "completion releases final spool pin");
}

void finish_request(SourceResourceChecks &checks, std::size_t mode, ProbeRangeRead read,
                    const std::shared_ptr<ProbeSourceIo> &io)
{
    if (mode == 0)
    {
        read.cancel();
        checks.expect(read.run().error() == Failure::cancelled,
                      "cancelled range is never a successful value");
    }
    if (mode == 1)
    {
        io->script_reads({ProbeIoFault::short_io, ProbeIoFault::failed});
        checks.expect(read.run().error() == Failure::unreadable,
                      "partial range read failure exposes no bytes");
        read.cancel();
        checks.expect(read.run().error() == Failure::consumed,
                      "failed range remains consumed after cancel");
    }
}

void verify_request_endings(SourceResourceChecks &checks)
{
    for (const std::size_t mode : {std::size_t{0}, std::size_t{1}, std::size_t{2}})
    {
        const auto io = std::make_shared<ProbeSourceIo>();
        auto sealed = make_source(checks, "abcdef", 6, io);
        if (!sealed)
        {
            continue;
        }
        auto read = sealed.value().request(0, 6);
        if (!read)
        {
            checks.expect(false, "accept request ending");
            continue;
        }
        sealed.value().release();
        finish_request(checks, mode, std::move(read.value()), io);
        checks.expect(absent(spool_path), "cancel/failure/destruction releases final request pin");
    }
}

void verify_ranges(SourceResourceChecks &checks)
{
    auto sealed = make_source(checks, "abcdef", 6, std::make_shared<ProbeSourceIo>());
    if (!sealed)
    {
        return;
    }
    checks.expect(sealed.value().request(7, 0).error() == Failure::invalid_range,
                  "offset beyond EOF is rejected");
    checks.expect(sealed.value().request(5, 2).error() == Failure::invalid_range,
                  "range beyond EOF is rejected by subtraction");
    checks.expect(sealed.value().request(1, (std::numeric_limits<std::size_t>::max)()).error() ==
                      Failure::invalid_range,
                  "range arithmetic cannot overflow");
    checks.expect(sealed.value().request(0, probe_source_block_bytes + 1).error() ==
                      Failure::invalid_range,
                  "oversized range is rejected without allocating its bytes");
    auto empty = sealed.value().request(6, 0);
    if (!empty)
    {
        checks.expect(false, "EOF empty request accepted");
        return;
    }
    const auto result = empty.value().run();
    checks.expect(result.has_value() && result.value().bytes().empty(), "EOF zero range succeeds");
    sealed.value().release();
}

void verify_path_identity(SourceResourceChecks &checks)
{
    auto old = make_source(checks, "old", 3, std::make_shared<ProbeSourceIo>());
    if (!old)
    {
        return;
    }
    auto read = old.value().request(0, 3);
    if (!read)
    {
        checks.expect(false, "old resource range");
        return;
    }
    checks.expect(DeleteFileW(input_path) != 0 && set_input("new"),
                  "copy completion releases original handle for path replacement");
    const auto before = read.value().run();
    checks.expect(before.has_value() && before.value().bytes() == "old",
                  "sealed old bytes survive original path replacement");
    old.value().release();
    auto fresh = make_source(checks, "new", 3, std::make_shared<ProbeSourceIo>());
    if (!fresh || !before)
    {
        checks.expect(false, "replacement identity fixture");
        return;
    }
    checks.expect(!fresh.value().id().same(before.value().id()),
                  "same path gets a new token while old result remains alive");
    fresh.value().release();
}

void verify_creation(SourceResourceChecks &checks)
{
    checks.expect(set_input("keep"), "creation fixture");
    {
        const FileHandle existing(CreateFileW(spool_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        checks.expect(existing.valid(), "create other owner's spool");
        DWORD written = 0;
        checks.expect(existing.valid() &&
                          WriteFile(existing.get(), "other", 5, &written, nullptr) != 0 &&
                          written == 5,
                      "write nonempty sentinel into another owner's spool");
    }
    const auto collision =
        ProbeSourceTask::start(input_path, spool_path, std::make_shared<ProbeSourceIo>());
    checks.expect(!collision && collision.error() == Failure::already_exists &&
                      source_matches("other", spool_path),
                  "collision never deletes another owner's spool");
    checks.expect(DeleteFileW(spool_path) != 0, "test owner removes its own collision fixture");
    const auto io = std::make_shared<ProbeSourceIo>();
    io->script_size(probe_source_limit_bytes + 1);
    const auto large = ProbeSourceTask::start(input_path, spool_path, io);
    checks.expect(!large && large.error() == Failure::too_large && absent(spool_path),
                  "injected oversized declaration is rejected before spool creation");
    const auto bad = ProbeSourceTask::start(input_path, L"source-resource-probe/missing/spool",
                                            std::make_shared<ProbeSourceIo>());
    checks.expect(!bad && bad.error() == Failure::create_failed, "spool creation failure is typed");
    const auto missing = ProbeSourceTask::start(L"source-resource-probe/missing-source", spool_path,
                                                std::make_shared<ProbeSourceIo>());
    checks.expect(!missing && missing.error() == Failure::unreadable && absent(spool_path),
                  "source open failure does not create spool");
}

void verify_sharing_and_moves(SourceResourceChecks &checks)
{
    checks.expect(set_input("abcdef"), "sharing fixture");
    auto initial =
        ProbeSourceTask::start(input_path, spool_path, std::make_shared<ProbeSourceIo>());
    if (!initial)
    {
        checks.expect(false, "start sharing task");
        return;
    }
    {
        const FileHandle writer(CreateFileW(input_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        const FileHandle outside(CreateFileW(spool_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        checks.expect(!writer.valid() && !outside.valid(),
                      "source writer and external spool read are refused");
    }
    auto moved = std::move(initial.value());
    checks.expect(initial.value().step(2).error() == Failure::unavailable,
                  "moved task cannot perform I/O");
    checks.expect(moved.step(6).has_value(), "moved task keeps resource ownership");
    auto sealed = moved.take();
    if (!sealed)
    {
        checks.expect(false, "take moved task");
        return;
    }
    auto initial_read = sealed.value().request(0, 6);
    if (!initial_read)
    {
        checks.expect(false, "accept move read");
        return;
    }
    auto moved_read = std::move(initial_read.value());
    checks.expect(initial_read.value().run().error() == Failure::consumed,
                  "moved request cannot run");
    sealed.value().release();
    checks.expect(moved_read.run().has_value() && absent(spool_path),
                  "moved request releases final pin after run");
}
void verify_closed_moves(SourceResourceChecks &checks)
{
    checks.expect(set_input("abcdef"), "cancelled move fixture");
    auto task = ProbeSourceTask::start(input_path, spool_path, std::make_shared<ProbeSourceIo>());
    if (!task)
    {
        checks.expect(false, "start closed move task");
        return;
    }
    task.value().cancel();
    auto moved = std::move(task.value());
    task.value().cancel();
    checks.expect(task.value().step(2).error() == Failure::unavailable &&
                      task.value().take().error() == Failure::unavailable,
                  "moving a cancelled task fixes the source to unavailable");
    checks.expect(moved.step(2).error() == Failure::cancelled,
                  "moving preserves the destination's cancelled state");
    auto original = make_source(checks, "abcdef", 6, std::make_shared<ProbeSourceIo>());
    if (!original)
    {
        return;
    }
    auto sealed = std::move(original.value());
    checks.expect(original.value().request(0, 1).error() == Failure::unavailable,
                  "moved owner cannot accept new requests");
    auto range = sealed.request(0, 6);
    if (!range)
    {
        checks.expect(false, "accept cancelled move range");
        return;
    }
    range.value().cancel();
    range.value().cancel();
    auto cancelled = std::move(range.value());
    range.value().cancel();
    checks.expect(range.value().run().error() == Failure::consumed,
                  "moving a cancelled range fixes the source to consumed");
    checks.expect(cancelled.run().error() == Failure::cancelled,
                  "moving preserves the cancelled range's destination state");
    sealed.release();
    checks.expect(absent(spool_path), "cancelled move values do not keep resource pins");
}

[[nodiscard]] std::expected<ProbeSealedSource, Failure>
ready_resource_at(SourceResourceChecks &checks, const wchar_t *spool)
{
    auto task = ProbeSourceTask::start(input_path, spool, std::make_shared<ProbeSourceIo>());
    checks.expect(task.has_value(), "start second small resource");
    if (!task)
    {
        return std::unexpected(task.error());
    }
    const auto step = task.value().step(6);
    checks.expect(step.has_value(), "copy second small resource");
    if (!step)
    {
        return std::unexpected(step.error());
    }
    return task.value().take();
}

void verify_task_assignment(SourceResourceChecks &checks)
{
    checks.expect(set_input("abcdef"), "task move assignment fixture");
    auto target = ProbeSourceTask::start(input_path, spool_path, std::make_shared<ProbeSourceIo>());
    auto source =
        ProbeSourceTask::start(input_path, second_spool_path, std::make_shared<ProbeSourceIo>());
    if (!target || !source)
    {
        checks.expect(false, "start task assignment pair");
        return;
    }
    target.value() = std::move(source.value());
    checks.expect(absent(spool_path) && !absent(second_spool_path),
                  "task assignment releases replaced spool and holds transferred spool");
    checks.expect(source.value().step(2).error() == Failure::unavailable,
                  "task assignment closes its source state");
    checks.expect(target.value().step(6).has_value(), "assigned task still owns source and spool");
    target.value().cancel();
    checks.expect(absent(second_spool_path), "assigned task cancellation closes transferred spool");
}

void verify_owner_assignment(SourceResourceChecks &checks)
{
    checks.expect(set_input("abcdef"), "owner assignment fixture");
    auto target = ready_resource_at(checks, spool_path);
    auto source = ready_resource_at(checks, second_spool_path);
    if (!target || !source)
    {
        return;
    }
    target.value() = std::move(source.value());
    checks.expect(absent(spool_path) && !absent(second_spool_path),
                  "owner assignment closes replaced spool and pins transferred spool");
    checks.expect(source.value().request(0, 1).error() == Failure::unavailable,
                  "assigned owner source cannot accept requests");
    target.value().release();
    checks.expect(absent(second_spool_path), "assigned owner release closes transferred spool");
}

void verify_range_assignment(SourceResourceChecks &checks)
{
    checks.expect(set_input("abcdef"), "range assignment fixture");
    auto first = ready_resource_at(checks, spool_path);
    auto second = ready_resource_at(checks, second_spool_path);
    if (!first || !second)
    {
        return;
    }
    auto target = first.value().request(0, 6);
    auto source = second.value().request(0, 6);
    if (!target || !source)
    {
        checks.expect(false, "accept assignment ranges");
        return;
    }
    first.value().release();
    second.value().release();
    target.value() = std::move(source.value());
    checks.expect(absent(spool_path) && !absent(second_spool_path),
                  "range assignment releases replaced pin and holds transferred pin");
    checks.expect(source.value().run().error() == Failure::consumed,
                  "range assignment closes source request");
    const auto result = target.value().run();
    checks.expect(result.has_value() && result.value().bytes() == "abcdef" &&
                      result.value().id().same(second.value().id()) && absent(second_spool_path),
                  "assigned request completes before dropping final transferred pin");
}
} // namespace

int main()
{
    SourceResourceChecks checks;
    if (CreateDirectoryW(folder, nullptr) == 0)
    {
        checks.expect(false, "fresh fixture folder required; previous artifacts preserved");
        return checks.finish();
    }
    verify_raw_bytes(checks);
    verify_short_read(checks);
    verify_faults(checks, ProbeIoFault::failed, false);
    verify_faults(checks, ProbeIoFault::zero, false);
    verify_faults(checks, ProbeIoFault::failed, true);
    verify_faults(checks, ProbeIoFault::short_io, true);
    verify_cancel(checks);
    verify_release_pin(checks);
    verify_request_endings(checks);
    verify_ranges(checks);
    verify_path_identity(checks);
    verify_creation(checks);
    verify_sharing_and_moves(checks);
    verify_closed_moves(checks);
    verify_task_assignment(checks);
    verify_owner_assignment(checks);
    verify_range_assignment(checks);
    checks.expect(absent(spool_path) && absent(second_spool_path), "both final spools are absent");
    checks.expect(DeleteFileW(input_path) != 0 && RemoveDirectoryW(folder) != 0,
                  "remove only the probe's own final input and empty fixture folder");
    return checks.finish();
}
