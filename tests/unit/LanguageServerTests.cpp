// 契約 --language-server（ADR 0160 の決定 1・5〜7・9・追補・Issue #994）。替え玉
// ScriptedLanguageServer で、起動 → `initialize` → 答え → `initialized`・失敗の値・再起動の
// 上限 3・tick の古さ・設定の on / off で start / stop・`:set lsp` の要求を確かめる。本物の
// clangd も窓も使わない。
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "ExLspRequest.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LanguageServerTick.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ServerCommand.hpp"
#include "ServerEvent.hpp"
#include "ServerExited.hpp"
#include "ServerFailed.hpp"
#include "ServerFailure.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view note_path = "C:\\work\\note.txt";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] std::string message(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

// lsp を on / off にし、cpp にだけ命令を書いた設定。
[[nodiscard]] SettingsReading lsp_settings(bool on)
{
    auto settings = core::default_editor_settings();
    settings.lsp = on;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

void hold_documents(Editing &editing)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string("int main() {}\n")});
    editing.files().hold_at(std::string(note_path), Bytes{std::string("note\n")});
}

// 出来事を仕込み、合図の意図で引かせる。
app::EditorFrame delivered(Editing &editing, app::ServerEvent event)
{
    editing.language_server().serve(std::move(event));
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

// 最後に保存した設定の lsp（保存していなければ値なし）。
[[nodiscard]] std::optional<bool> saved_lsp(Editing &editing)
{
    const auto &written = editing.settings().written();
    return written.has_value() ? std::optional<bool>{written.value().lsp} : std::nullopt;
}

[[nodiscard]] const std::string &last_sent(Editing &editing)
{
    return editing.language_server().sent().back();
}

// 開いて、`initialize` に答え、`initialized` まで進める。
void started(Editing &editing)
{
    hold_documents(editing);
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{}}})"}));
}

// 設定の 1 行を起こす要求にする（決定 7）。
void verify_server_command()
{
    const auto root = path_of("C:\\work");
    const auto launch = app::server_launch_of(clangd_command, root);
    expect(launch.has_value() && launch.value().executable.text() == "C:\\LLVM\\bin\\clangd.exe" &&
               launch.value().arguments == std::vector<std::string>{"--log=error"} &&
               launch.value().root == root,
           "a quoted executable keeps its spaces and the rest are arguments");
    const auto spaced = app::server_launch_of(R"(  "C:\Program Files\x.exe"  a  "b c"  )", root);
    expect(spaced.has_value() && spaced.value().executable.text() == "C:\\Program Files\\x.exe" &&
               spaced.value().arguments == std::vector<std::string>{"a", "b c"},
           "blanks split words outside quotes only");
    expect(!app::server_launch_of("", root).has_value() &&
               !app::server_launch_of("   ", root).has_value() &&
               !app::server_launch_of(R"("C:\x.exe)", root).has_value(),
           "an empty command or an unclosed quote launches nothing");
}

// `:set lsp` `nolsp` `lsp?` は要求（決定 7）。
void verify_ex_lsp_option()
{
    const auto settings = core::default_editor_settings();
    const auto request = [&settings](std::string_view text)
    {
        const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
        return result.has_value() ? result.value().lsp : std::nullopt;
    };
    expect(request("set lsp") == std::optional{core::ExLspRequest::enable} &&
               request("set nolsp") == std::optional{core::ExLspRequest::disable} &&
               request("set lsp?") == std::optional{core::ExLspRequest::show} &&
               !request("set hlsearch").has_value(),
           "set lsp, nolsp and lsp? are the three requests");
    const auto saved = core::evaluate_ex("set lsp", settings, core::Appearance::dark);
    expect(saved.has_value() && !saved.value().settings.has_value(),
           "the controller saves the switch, not the Ex result");
}

// 起動 → `initialize` → 答え → `initialized`（決定 5・6）。打鍵は port を呼ばない。
void verify_lifecycle()
{
    Editing editing(lsp_settings(true));
    hold_documents(editing);
    auto &server = editing.language_server();
    expect(server.touches() == 0, "nothing starts before a document with a command is active");
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    expect(server.launches().size() == 1 &&
               server.launches().front().executable.text() == "C:\\LLVM\\bin\\clangd.exe" &&
               server.launches().front().root.text() == "C:\\work" && server.sent().empty(),
           "opening a C++ file starts its server in its folder and sends nothing yet");
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    expect(server.sent().size() == 1 && contains(last_sent(editing), R"("method":"initialize")") &&
               contains(last_sent(editing), R"("id":1)") &&
               contains(last_sent(editing), R"("positionEncodings":["utf-8","utf-16"])"),
           "the started server is asked to initialize with both position encodings");
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8"}}})"}));
    expect(server.sent().size() == 2 && contains(last_sent(editing), R"("method":"initialized")") &&
               !contains(last_sent(editing), R"("id")"),
           "the answer to initialize is followed by the initialized notification");
    const std::size_t before = server.touches();
    applied(editing.controller(), app::InsertText{"x"});
    applied(editing.controller(), app::InsertText{"y"});
    expect(server.touches() == before, "typing never calls the language server port in L0");
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":"w1","method":"window/workDoneProgress/create","params":{}})"}));
    expect(contains(last_sent(editing), "-32601") && contains(last_sent(editing), R"("id":"w1")"),
           "a request from the server is refused with MethodNotFound");
}

// on / off で start / stop（決定 6・7）。off は `shutdown` → 答え → `exit` → stop。
void verify_switch()
{
    Editing editing(lsp_settings(true));
    started(editing);
    auto &server = editing.language_server();
    const auto off = run_ex(editing.controller(), "set nolsp");
    expect(message(off) == "lsp=off" && contains(last_sent(editing), R"("method":"shutdown")") &&
               contains(last_sent(editing), R"("id":2)") && server.stops() == 0 &&
               saved_lsp(editing) == std::optional<bool>{false},
           "set nolsp saves the switch and asks the server to shut down");
    static_cast<void>(
        delivered(editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":2,"result":null})"}));
    expect(contains(last_sent(editing), R"("method":"exit")") && server.stops() == 1,
           "the answer to shutdown is followed by exit and stop");
    const std::size_t quiet = server.touches();
    applied(editing.controller(), app::InsertText{"x"});
    static_cast<void>(editing.controller().apply_frame(app::LanguageServerTick{}));
    static_cast<void>(editing.controller().apply_frame(app::LanguageServerTick{}));
    expect(server.touches() == quiet + 1 && server.expirations() == 1,
           "off, only the first tick after stop reaches the port to expire the retired child");
    const auto on = run_ex(editing.controller(), "set lsp");
    expect(message(on) == "lsp=on" && server.launches().size() == 2 &&
               saved_lsp(editing) == std::optional<bool>{true},
           "set lsp saves the switch and starts the server again");
    const auto asked = run_ex(editing.controller(), "set lsp?");
    expect(message(asked) == "lsp=on" && server.launches().size() == 2,
           "set lsp? only tells the current value");
}

// アクティブな文書の言語に命令が無くなれば止め、戻れば起こす（決定 6）。
void verify_language_follow()
{
    Editing editing(lsp_settings(true));
    started(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(note_path)});
    expect(contains(last_sent(editing), R"("method":"shutdown")"),
           "a document without a command shuts the server down");
    static_cast<void>(
        delivered(editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":2,"result":null})"}));
    expect(server.stops() == 1 && server.launches().size() == 1,
           "plain text starts no server after the shutdown");
    applied(editing.controller(), app::SwitchTab{0});
    expect(server.launches().size() == 2, "coming back to the C++ tab starts the server again");
}

// 失敗は 1 行の知らせ・自動の再起動は 3 回まで・超えたら止まり `:set lsp` で再開（決定 6）。
void verify_restart_limit()
{
    Editing editing(lsp_settings(true));
    started(editing);
    auto &server = editing.language_server();
    const auto first = delivered(editing, app::ServerExited{1});
    expect(message(first) == "言語サーバーが終わりました: cpp" && server.stops() == 1 &&
               server.launches().size() == 2,
           "an exited server is told in one line and started again");
    static_cast<void>(delivered(editing, app::ServerFailed{app::ServerFailure::read_failed}));
    static_cast<void>(delivered(editing, app::ServerExited{3}));
    expect(server.launches().size() == 4, "a failed value counts like an exit");
    const auto last = delivered(editing, app::ServerExited{4});
    expect(server.launches().size() == 4 &&
               message(last) == "言語サーバーを止めました（:set lsp で再開）: cpp",
           "after three restarts the fourth failure gives up");
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"x"});
    expect(server.launches().size() == 4, "a given-up server is not started by other intents");
    static_cast<void>(run_ex(editing.controller(), "set lsp"));
    expect(server.launches().size() == 5, "set lsp starts it again by hand");
}

// 起こせない命令は 1 行知らせて止まる（自動では起こし直さない）。
void verify_start_refused()
{
    Editing editing(lsp_settings(true));
    hold_documents(editing);
    auto &server = editing.language_server();
    server.refuse(app::ServerFailure::not_found);
    const auto opened = editing.controller().apply_frame(app::OpenDocument{path_of(source_path)});
    expect(message(opened) == "言語サーバーを起こせませんでした: cpp" &&
               server.launches().size() == 1,
           "a refused start is told in one line");
    applied(editing.controller(), app::InsertText{"x"});
    expect(server.launches().size() == 1, "a refused start is not tried again by itself");
}

// 30 秒を超えた要求（2 回の tick）は `$/cancelRequest` を送って捨てる（決定 6・追補）。
void verify_staleness()
{
    Editing editing(lsp_settings(true));
    hold_documents(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(editing.controller().apply_frame(app::LanguageServerTick{}));
    expect(server.sent().size() == 1 && server.launches().size() == 1,
           "one tick does not make the initialize request stale");
    const auto stale = editing.controller().apply_frame(app::LanguageServerTick{});
    expect(contains(server.sent().back(), R"("method":"$/cancelRequest")") &&
               contains(server.sent().back(), R"("params":{"id":1})") && server.stops() == 1 &&
               server.launches().size() == 2 && message(stale) == "言語サーバーが答えません: cpp",
           "a silent initialize is cancelled after two ticks and counted as a failure");
    static_cast<void>(delivered(
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{}}})"}));
    expect(!contains(server.sent().back(), R"("initialized")"),
           "a late answer to the cancelled request is ignored");

    Editing closing(lsp_settings(true));
    started(closing);
    static_cast<void>(run_ex(closing.controller(), "set nolsp"));
    const std::size_t sent = closing.language_server().sent().size();
    static_cast<void>(closing.controller().apply_frame(app::LanguageServerTick{}));
    static_cast<void>(closing.controller().apply_frame(app::LanguageServerTick{}));
    expect(closing.language_server().stops() == 1 &&
               closing.language_server().sent().size() == sent,
           "a server that never answers shutdown is stopped without exit");
}

// tick は使う人の操作ではない（決定 6）。知らせも Ctrl+Tab の歩きも動かさない。
void verify_tick_is_not_an_operation()
{
    Editing editing(lsp_settings(true));
    started(editing);
    applied(editing.controller(), app::OpenDocument{path_of(note_path)});
    const auto told = run_ex(editing.controller(), "set lsp?");
    const auto ticked = editing.controller().apply_frame(app::LanguageServerTick{});
    expect(message(told) == "lsp=on" && message(ticked) == "lsp=on",
           "a tick keeps the one-line notice");
    applied(editing.controller(), app::WalkRecentTab{core::TabStep::next});
    static_cast<void>(editing.controller().apply_frame(app::LanguageServerTick{}));
    expect(editing.controller().tab_walking(), "a tick keeps the Ctrl+Tab walk going");
}

// オフのときは起動・開く・切り替え・打鍵・保存・合図で port を呼ばない（決定 9）。
void verify_quiet_when_off()
{
    Editing editing(lsp_settings(false));
    hold_documents(editing);
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    applied(editing.controller(), app::OpenDocument{path_of(note_path)});
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"x"});
    applied(editing.controller(), app::WorkCompleted{});
    applied(editing.controller(), app::LanguageServerTick{});
    expect(editing.language_server().touches() == 0,
           "with lsp off nothing reaches the language server port");
}
} // namespace

void verify_language_server_contracts()
{
    verify_server_command();
    verify_ex_lsp_option();
    verify_lifecycle();
    verify_switch();
    verify_language_follow();
    verify_restart_limit();
    verify_start_refused();
    verify_staleness();
    verify_tick_is_not_an_operation();
    verify_quiet_when_off();
}

void verify_language_server_scope()
{
    verify_language_server_contracts();
}
} // namespace nenenib::tests
