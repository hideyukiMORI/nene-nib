// 契約 --language-server の席の部分（ADR 0160 / 0161 の 2026-11-20 の追補・LSP 段 S2・#1909）。
// 言語サーバーを言語ごとに持ち、タブの切り替えで止めない・失敗は言語ごと・答えの id は席ごと・
// 席が満ちたら最も前に使った席を譲る・`:set ft=` で言語が変われば前の席へ didClose。替え玉
// ScriptedLanguageServer で確かめる。本物のサーバーも窓も使わない。
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageServerSlot.hpp"
#include "OpenDocument.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "ServerExited.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
using Slot = app::LanguageServerSlot;

constexpr std::string_view cpp_path = "C:\\work\\main.cpp";
constexpr std::string_view python_path = "C:\\work\\app.py";
constexpr std::string_view note_path = "C:\\work\\README.md";
constexpr std::string_view rust_path = "C:\\work\\lib.rs";
constexpr std::string_view go_path = "C:\\work\\main.go";
constexpr std::string_view lua_path = "C:\\work\\init.lua";
constexpr std::string_view cpp_uri = "file:///C:/work/main.cpp";
constexpr std::string_view running_answer =
    R"({"capabilities":{"textDocumentSync":2,"hoverProvider":true}})";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

// lsp をオンにし、cpp と python（と more の言語）に命令を書いた設定。markdown には書かない。
[[nodiscard]] SettingsReading slot_settings(std::vector<std::string> more = {})
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", R"(C:\LLVM\bin\clangd.exe)"},
                             {"python", R"(C:\Python\jedi.exe)"}};
    for (std::string &name : more)
    {
        settings.lsp_commands.emplace_back(name, R"(C:\Servers\)" + name + ".exe");
    }
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

void hold_all(Editing &editing)
{
    for (const std::string_view path :
         {cpp_path, python_path, note_path, rust_path, go_path, lua_path})
    {
        editing.files().hold_at(std::string(path), Bytes{std::string("x\n")});
    }
}

// 席 slot の最後の `initialize` の id。
[[nodiscard]] std::int64_t initialize_id(const ScriptedLanguageServer &server, Slot slot)
{
    std::int64_t id = 0;
    for (const std::string &body : server.sent_to(slot))
    {
        constexpr std::string_view key = R"("id":)";
        if (contains(body, R"("method":"initialize")"))
        {
            id = std::stoll(body.substr(body.find(key) + key.size()));
        }
    }
    return id;
}

// path を開き、起きた席を ServerStarted → `initialize` の答え → `initialized` まで進める。
void opened_running(Editing &editing, std::string_view path, Slot slot)
{
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(path)});
    server.serve(slot, app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    server.answer(slot, initialize_id(server, slot), running_answer);
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

[[nodiscard]] std::size_t count_method(const std::vector<std::string> &bodies,
                                       std::string_view method)
{
    const std::string key = R"("method":")" + std::string{method} + "\"";
    return static_cast<std::size_t>(std::ranges::count_if(bodies, [&key](const std::string &body)
                                                          { return contains(body, key); }));
}

// 命令の無い言語（Markdown）のタブへ移っても止めず、診断も残り、戻っても起こし直さない
// （verify_language_follow の新しい形・穴 1）。
void verify_language_follow()
{
    Editing editing(slot_settings());
    hold_all(editing);
    opened_running(editing, cpp_path, Slot::first);
    auto &server = editing.language_server();
    server.publish(
        Slot::first, cpp_uri, 0,
        R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"severity":1,"message":"bad"}])");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    const std::size_t sent = server.sent().size();
    applied(editing.controller(), app::OpenDocument{path_of(note_path)});
    applied(editing.controller(), app::InsertText{"y"});
    expect(server.stops() == 0 && server.sent().size() == sent && server.launches().size() == 1,
           "a tab of a language without a command neither stops the server nor closes the "
           "document");
    const auto back = editing.controller().apply_frame(app::SwitchTab{0});
    expect(back.diagnostic_counts.errors == 1 && server.launches().size() == 1 &&
               count_method(server.sent(), "textDocument/didOpen") == 1,
           "coming back finds the diagnostics kept and the same server without a new didOpen");
}

// C++ と Python のタブを並べれば 2 本が同時に動き、didChange は文書の言語の席へ行く。
void verify_two_languages_at_once()
{
    Editing editing(slot_settings());
    hold_all(editing);
    opened_running(editing, cpp_path, Slot::first);
    opened_running(editing, python_path, Slot::second);
    auto &server = editing.language_server();
    expect(server.started_slots() == std::vector<Slot>{Slot::first, Slot::second} &&
               server.stops() == 0 &&
               server.launches().at(1).executable.text() == "C:\\Python\\jedi.exe",
           "a Python tab starts its own server in the second slot beside the C++ one");
    expect(count_method(server.sent_to(Slot::second), "textDocument/didOpen") == 1 &&
               contains(server.sent_to(Slot::second).back(), R"("languageId":"python")") &&
               count_method(server.sent_to(Slot::first), "textDocument/didOpen") == 1,
           "each document is opened on the server of its language only");
    applied(editing.controller(), app::InsertText{"p"});
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"c"});
    expect(count_method(server.sent_to(Slot::second), "textDocument/didChange") == 1 &&
               count_method(server.sent_to(Slot::first), "textDocument/didChange") == 1 &&
               server.stops() == 0,
           "edits go to the slot of the document's language and switching stops nothing");
}

// 答えの id の表は席ごと。ほかの席から届いた同じ id の答えは使わない（取り違えない）。
void verify_answers_by_slot()
{
    Editing editing(slot_settings());
    hold_all(editing);
    opened_running(editing, python_path, Slot::first);
    opened_running(editing, cpp_path, Slot::second);
    auto &server = editing.language_server();
    applied(editing.controller(), app::QueryLanguage{core::LanguageQueryKind::hover});
    const auto asked = server.requests("textDocument/hover");
    expect(asked.size() == 1 &&
               count_method(server.sent_to(Slot::second), "textDocument/hover") == 1,
           "the hover of the C++ document is asked of the C++ slot");
    server.answer(Slot::first, asked.back(), R"({"contents":"wrong"})");
    const auto crossed = editing.controller().apply_frame(app::WorkCompleted{});
    expect(!crossed.hover.has_value(), "an answer from another slot with the same id is ignored");
    server.answer(Slot::second, asked.back(), R"({"contents":"int main"})");
    const auto right = editing.controller().apply_frame(app::WorkCompleted{});
    expect(right.hover == std::optional<std::string>{"int main"},
           "the answer from the slot that was asked is used");
}

// 失敗の数と given_up は言語ごと。Python が諦めても C++ は動き続け、`:set lsp` で全部を戻す。
void verify_failures_by_language()
{
    Editing editing(slot_settings());
    hold_all(editing);
    opened_running(editing, cpp_path, Slot::first);
    opened_running(editing, python_path, Slot::second);
    auto &server = editing.language_server();
    for (std::size_t round = 0; round < 4; ++round)
    {
        server.serve(Slot::second, app::ServerExited{1});
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    }
    expect(server.launches().size() == 5 &&
               std::ranges::all_of(server.stopped_slots(),
                                   [](Slot slot) { return slot == Slot::second; }),
           "the Python server is restarted three times and then given up, alone");
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"c"});
    expect(count_method(server.sent_to(Slot::first), "textDocument/didChange") == 1,
           "the C++ server keeps running after Python gave up");
    applied(editing.controller(), app::SwitchTab{1});
    expect(server.launches().size() == 5, "a given-up language is not started by switching");
    static_cast<void>(run_ex(editing.controller(), "set lsp"));
    expect(server.launches().size() == 6, "set lsp clears the given-up state of every language");
}

// 席が満ちたら、アクティブな文書で最も前に使った席を `shutdown` して譲る（didClose
// は譲る席の文書）。
void verify_slot_yield()
{
    Editing editing(slot_settings({"rust", "go", "lua"}));
    hold_all(editing);
    opened_running(editing, cpp_path, Slot::first);
    opened_running(editing, python_path, Slot::second);
    opened_running(editing, rust_path, Slot::third);
    opened_running(editing, go_path, Slot::fourth);
    auto &server = editing.language_server();
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::OpenDocument{path_of(lua_path)});
    const auto python = server.sent_to(Slot::second);
    expect(server.launches().size() == 4 && count_method(python, "shutdown") == 1 &&
               count_method(python, "textDocument/didClose") == 1 &&
               count_method(server.sent_to(Slot::first), "shutdown") == 0,
           "the fifth language asks the least recently used slot to shut down");
    std::int64_t shutdown_id = 0;
    for (const std::string &body : python)
    {
        if (contains(body, R"("method":"shutdown")"))
        {
            shutdown_id = std::stoll(body.substr(body.find(R"("id":)") + 5));
        }
    }
    server.answer(Slot::second, shutdown_id, "null");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(server.launches().size() == 5 && server.started_slots().back() == Slot::second &&
               server.launches().back().executable.text() == "C:\\Servers\\lua.exe",
           "after the shutdown answer the freed slot starts the new language");
}

// `:set ft=` で言語が変われば、前の席へ didClose し、その席は止めない。
void verify_filetype_change()
{
    Editing editing(slot_settings());
    hold_all(editing);
    opened_running(editing, cpp_path, Slot::first);
    auto &server = editing.language_server();
    static_cast<void>(run_ex(editing.controller(), "set ft=markdown"));
    expect(count_method(server.sent_to(Slot::first), "textDocument/didClose") == 1 &&
               server.stops() == 0 && count_method(server.sent(), "shutdown") == 0,
           "a changed filetype closes the document on the old server without stopping it");
}
} // namespace

void verify_language_server_slots()
{
    verify_language_follow();
    verify_two_languages_at_once();
    verify_answers_by_slot();
    verify_failures_by_language();
    verify_slot_yield();
    verify_filetype_change();
}
} // namespace nenenib::tests
