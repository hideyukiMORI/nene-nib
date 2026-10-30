// Issue #1020（親 #994）: 言語サーバーの adapter（Win32LanguageServer）を、本物の子プロセス・
// 名前つきパイプ・Job Object と替え玉のサーバー nib_fake_language_server で通す（ADR 0160 の決定
// 2・6・9・ADR 0126）。替え玉の経路は CTest が第 1 引数で渡す。時刻は読まない（Sleep・時計・
// 制限時間つきの待ちを書かない）。試験のスレッドが pump の代わりに waitable_handles を
// WaitForMultipleObjects で待ち、立った handle を completed に返して collect する。
// 止まらないときは CTest の制限時間で落ちる。
#include "FilePath.hpp"
#include "ServerCommandLine.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerLaunch.hpp"
#include "Win32LanguageServer.hpp"

#include <windows.h>

#include <tlhelp32.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace adapters = nenenib::adapters::win32;
using nenenib::application::ServerEvent;
using nenenib::application::ServerExited;
using nenenib::application::ServerFailed;
using nenenib::application::ServerFailure;
using nenenib::application::ServerLaunch;
using nenenib::application::ServerMessage;
using nenenib::application::ServerStarted;
using Events = std::vector<ServerEvent>;
using Arguments = std::vector<std::string>;
using Finished = std::function<bool(const Events &)>;

constexpr std::size_t small_read = 4096;
constexpr std::size_t echoed_bytes = 70000;

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

std::string &fake_server()
{
    static std::string path;
    return path;
}

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

ServerLaunch launch_of(Arguments arguments)
{
    return ServerLaunch{path_of(fake_server()), std::move(arguments), path_of(".")};
}

template <typename Event> std::size_t count_of(const Events &events)
{
    std::size_t count = 0;
    for (const auto &event : events)
    {
        count += std::holds_alternative<Event>(event) ? 1 : 0;
    }
    return count;
}

std::vector<std::string> bodies(const Events &events)
{
    std::vector<std::string> found;
    for (const auto &event : events)
    {
        if (std::holds_alternative<ServerMessage>(event))
        {
            found.push_back(std::get<ServerMessage>(event).body);
        }
    }
    return found;
}

std::optional<std::int32_t> exit_code(const Events &events)
{
    for (const auto &event : events)
    {
        if (std::holds_alternative<ServerExited>(event))
        {
            return std::get<ServerExited>(event).code;
        }
    }
    return std::nullopt;
}

bool failed_with(const Events &events, ServerFailure failure)
{
    for (const auto &event : events)
    {
        if (std::holds_alternative<ServerFailed>(event) &&
            std::get<ServerFailed>(event).failure == failure)
        {
            return true;
        }
    }
    return false;
}

// 1 つの handle が立つまで待って adapter に返す。待つものが無ければ false。
bool pump_once(adapters::Win32LanguageServer &server)
{
    const auto span = server.waitable_handles();
    const std::vector<HANDLE> handles(span.begin(), span.end());
    if (handles.empty())
    {
        return false;
    }
    const DWORD signaled =
        WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);
    const DWORD index = signaled - WAIT_OBJECT_0;
    if (index >= handles.size())
    {
        expect(false, "the waitable handles can be waited on");
        return false;
    }
    server.completed(handles[index]);
    return true;
}

// 出来事を集めながら、finished が満たされるか待つものが無くなるまで pump を回す。
Events pump_until(adapters::Win32LanguageServer &server, const Finished &finished)
{
    Events events = server.collect();
    while (!finished(events) && pump_once(server))
    {
        for (auto &event : server.collect())
        {
            events.push_back(std::move(event));
        }
    }
    return events;
}

// 退く子を含めて待つものが無くなるまで pump を回し、出来事は捨てる。
void pump_all(adapters::Win32LanguageServer &server)
{
    while (pump_once(server))
    {
        static_cast<void>(server.collect());
    }
}

// 子が終わっている（終わるまで待つ。止まらなければ CTest の制限時間で落ちる）。
bool ended(HANDLE child)
{
    return child != nullptr && WaitForSingleObject(child, INFINITE) == WAIT_OBJECT_0;
}

// 子がまだ走っている（待たずに見る）。
bool running(HANDLE child)
{
    return child != nullptr && WaitForSingleObject(child, 0) == WAIT_TIMEOUT;
}

Finished messages(std::size_t count)
{
    return [count](const Events &events) { return bodies(events).size() >= count; };
}

bool exited(const Events &events)
{
    return exit_code(events).has_value();
}

bool any_failure(const Events &events)
{
    return count_of<ServerFailed>(events) > 0;
}

bool fake_child(const PROCESSENTRY32W &entry)
{
    return entry.th32ParentProcessID == GetCurrentProcessId() &&
           CompareStringOrdinal(static_cast<const wchar_t *>(entry.szExeFile), -1,
                                L"nib_fake_language_server.exe", -1, TRUE) == CSTR_EQUAL;
}

// まだ走っている子なら、終わりを待てる handle。終わった子と開けない子は nullptr。
HANDLE open_running(DWORD process_id)
{
    const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, process_id);
    if (process != nullptr && WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
    {
        CloseHandle(process);
        return nullptr;
    }
    return process;
}

// 試験のプロセスが起こした替え玉の子のうち走っているものを 1 つ、終わりを待てる形で開く。
HANDLE open_fake_child()
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    HANDLE found = nullptr;
    for (BOOL more = Process32FirstW(snapshot, &entry); more != 0 && found == nullptr;
         more = Process32NextW(snapshot, &entry))
    {
        found = fake_child(entry) ? open_running(entry.th32ProcessID) : nullptr;
    }
    CloseHandle(snapshot);
    return found;
}

void verify_lifecycle()
{
    std::size_t arrivals = 0;
    adapters::Win32LanguageServer server([&arrivals] { ++arrivals; });
    expect(server.start(launch_of({})).has_value(), "the fake server starts");
    expect(arrivals == 1, "the start is signalled once");
    auto events = server.collect();
    expect(count_of<ServerStarted>(events) == 1, "the start is one ServerStarted");
    expect(!server.waitable_handles().empty(),
           "a started server has a read and the process to wait on");
    server.send(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    events = pump_until(server, messages(1));
    const auto answer = bodies(events);
    expect(answer.size() == 1 && answer.front().find(R"("id":1,)") != std::string::npos &&
               answer.front().find(R"("positionEncoding":"utf-8")") != std::string::npos,
           "initialize is answered with the same id and the capabilities");
    server.send(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    server.send(R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})");
    events = pump_until(server, messages(1));
    expect(bodies(events).size() == 1 &&
               bodies(events).front() == R"({"jsonrpc":"2.0","id":2,"result":null})",
           "initialized is silent and shutdown is answered with result null");
    server.send(R"({"jsonrpc":"2.0","method":"exit"})");
    events = pump_until(server, exited);
    expect(exit_code(events) == std::optional<std::int32_t>{0}, "exit ends the server with code 0");
    expect(server.waitable_handles().empty(), "an exited server has nothing to wait on");
    expect(arrivals >= 4, "each batch of events is signalled");
}

void verify_echo_across_reads()
{
    adapters::Win32LanguageServer server([] {}, small_read);
    expect(server.start(launch_of({"--echo"})).has_value(), "the echo server starts");
    const std::string body = R"({"text":")" + std::string(echoed_bytes, 'a') + R"("})";
    server.send(body);
    const auto events = pump_until(server, messages(1));
    expect(bodies(events) == std::vector<std::string>{body},
           "a body larger than one read and the pipe buffer comes back as one message");
    server.stop();
}

void verify_two_messages_in_one_write()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--echo", "--double"})).has_value(),
           "the doubling echo server starts");
    server.send("first");
    const auto events = pump_until(server, messages(2));
    expect(bodies(events) == std::vector<std::string>{"first", "first"},
           "two frames in one write are two messages");
    server.stop();
}

void verify_crash()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--die-after", "2"})).has_value(), "the dying server starts");
    server.send(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    server.send(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    const auto events = pump_until(server, exited);
    expect(bodies(events).size() == 1, "the answer before the crash is delivered");
    expect(exit_code(events) == std::optional<std::int32_t>{3},
           "the crash is ServerExited with the code 3");
    server.send(R"({"jsonrpc":"2.0","method":"exit"})");
    expect(failed_with(server.collect(), ServerFailure::write_failed),
           "a send after the exit is write_failed");
    expect(server.waitable_handles().empty(), "the crashed server has nothing to wait on");
}

void verify_not_found()
{
    std::size_t arrivals = 0;
    adapters::Win32LanguageServer server([&arrivals] { ++arrivals; });
    const auto started =
        server.start(ServerLaunch{path_of(fake_server() + ".missing.exe"), {}, path_of(".")});
    expect(!started.has_value() && started.error() == ServerFailure::not_found,
           "a missing absolute executable is not_found");
    expect(arrivals == 0 && server.collect().empty(), "a failed start has no events");
    expect(server.waitable_handles().empty(), "a failed start has nothing to wait on");
    server.send("{}");
    expect(failed_with(server.collect(), ServerFailure::write_failed),
           "a send without a server is write_failed");
}

// 作業フォルダ（ビルドの置き場）には替え玉があるが、相対の経路は解決しない（ADR 0160 の追補）。
void verify_relative_is_not_found()
{
    adapters::Win32LanguageServer server([] {});
    const auto started =
        server.start(ServerLaunch{path_of("nib_fake_language_server.exe"), {}, path_of(".")});
    expect(!started.has_value() && started.error() == ServerFailure::not_found,
           "a relative executable is not_found even when it exists in the working folder");
    expect(server.waitable_handles().empty(), "a relative start has nothing to wait on");
}

void verify_too_long()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--big", "17000000"})).has_value(), "the big server starts");
    const auto events = pump_until(server, any_failure);
    expect(failed_with(events, ServerFailure::message_too_long),
           "a 17 MB body is message_too_long");
    expect(bodies(events).empty(), "the too long body is not delivered");
    const HANDLE child = open_fake_child();
    expect(running(child), "the blocked fake server is still running");
    server.stop();
    pump_all(server);
    expect(ended(child),
           "closing the pipes ends a blocked writer and the retired child is collected");
    CloseHandle(child);
}

// stop は止まらない子を待たずに戻り、expire_retired が Job Object ごと殺す（ADR 0160 の追補）。
// 空行の来ない見出しが 8 KiB を超えたら header_too_long（ADR 0160 の決定 3・#1022）。本文は
// 届かず、以後は読まない。
void verify_header_too_long()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--endless-header", "9000"})).has_value(),
           "the endless header server starts");
    const auto events = pump_until(server, any_failure);
    expect(failed_with(events, ServerFailure::header_too_long),
           "a header over 8 KiB without its blank line is header_too_long");
    expect(bodies(events).empty(), "nothing is delivered from the endless header");
    server.stop();
}

void verify_stop_does_not_wait()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--hang"})).has_value(), "the lingering server starts");
    const HANDLE child = open_fake_child();
    expect(running(child), "the lingering server is running");
    server.stop();
    expect(running(child) && server.waitable_handles().size() == 1,
           "stop returns at once and the retired child stays on the wait list");
    expect(server.collect().empty(), "stop discards the events collected so far");
    server.expire_retired();
    expect(ended(child), "expire_retired leaves no child behind");
    expect(server.waitable_handles().empty(), "nothing is left to wait on after expire_retired");
    CloseHandle(child);
}

// 標準入力を読まない子へ 1 MiB を送り、書きが出ている間に stop しても止まらない。
void verify_stop_cancels_a_pending_write()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--deaf"})).has_value(), "the deaf server starts");
    const HANDLE child = open_fake_child();
    server.send(std::string(std::size_t{1} << 20U, 'w'));
    expect(server.waitable_handles().size() == 3, "the large write is pending");
    server.stop();
    expect(running(child), "stop returns with the pending write cancelled");
    server.expire_retired();
    expect(ended(child), "the deaf child is not left behind");
    CloseHandle(child);
}

void verify_restart()
{
    adapters::Win32LanguageServer server([] {});
    expect(server.start(launch_of({"--echo"})).has_value(), "the first server starts");
    const HANDLE first = open_fake_child();
    expect(server.start(launch_of({"--echo"})).has_value(), "a second start replaces the first");
    server.send("again");
    const auto events = pump_until(server, messages(1));
    expect(count_of<ServerStarted>(events) == 1 &&
               bodies(events) == std::vector<std::string>{"again"},
           "the replaced server answers once and the first start event is discarded");
    expect(ended(first), "the first server ends on its own after its pipes close");
    CloseHandle(first);
}

void verify_command_line()
{
    expect(adapters::server_command_line(L"C:\\a b\\s.exe", {"--x", "two words", R"(q"uote)",
                                                             R"(dir\)", R"(end dir\)", ""}) ==
               LR"("C:\a b\s.exe" --x "two words" "q\"uote" dir\ "end dir\\" "")",
           "arguments are quoted the way CommandLineToArgvW reads them back");
    const auto broken = adapters::server_command_line(L"C:\\s.exe", {"ok", "\xff"});
    expect(!broken.has_value() && broken.error() == ServerFailure::cannot_start,
           "an argument that is not UTF-8 is cannot_start");
}
// 合図を結ぶ前（合成ルートでは窓の前・controller の構築の中の start）に溜まった出来事は、bind が
// 1 回知らせる（#994 の独立レビュー R1-1）。溜まりが空なら bind は知らせない。
void verify_bind_after_start()
{
    adapters::Win32LanguageServer server({});
    expect(server.start(launch_of({})).has_value(), "the fake server starts before the signal");
    std::size_t arrivals = 0;
    server.bind([&arrivals] { ++arrivals; });
    expect(arrivals == 1, "binding after a pending ServerStarted signals once");
    expect(count_of<ServerStarted>(server.collect()) == 1, "the pending start is collected");
    server.bind([&arrivals] { ++arrivals; });
    expect(arrivals == 1, "binding with nothing pending does not signal");
    server.stop();
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: nib_language_server_tests <nib_fake_language_server.exe>\n");
        return 2;
    }
    fake_server() = std::span<char *const>(argv, static_cast<std::size_t>(argc))[1];
    verify_command_line();
    verify_lifecycle();
    verify_bind_after_start();
    verify_echo_across_reads();
    verify_two_messages_in_one_write();
    verify_crash();
    verify_not_found();
    verify_relative_is_not_found();
    verify_too_long();
    verify_header_too_long();
    verify_stop_does_not_wait();
    verify_stop_cancels_a_pending_write();
    verify_restart();
    std::printf("Language server adapter: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
