// 契約 --language-server の席の共有の部分（ADR 0160 / 0161 の 2026-11-24 の追補・#1952）。席の鍵は
// （命令の行・root）で、c と cpp のように同じ命令の行の言語は子を 1 本だけ起こして共有する。
// `didOpen` の languageId は文書ごと・失敗と given_up
// は席（命令の行）ごと・共有の子が落ちたら両方の
// 言語の文書が開き直される・片方の言語の文書を閉じても子は生きる・席の上限は子の数で数える。
// 替え玉 ScriptedLanguageServer で確かめる。本物のサーバーも窓も使わない。
#include "CloseTab.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LanguageServerSlot.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ServerExited.hpp"
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
constexpr std::string_view c_path = "C:\\work\\util.c";
constexpr std::string_view other_c_path = "C:\\other\\far.c";
constexpr std::string_view ts_path = "C:\\work\\app.ts";
constexpr std::string_view js_path = "C:\\work\\app.js";
constexpr std::string_view python_path = "C:\\work\\app.py";
constexpr std::string_view rust_path = "C:\\work\\lib.rs";
constexpr std::string_view clangd = R"(C:\LLVM\bin\clangd.exe)";
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

// lsp をオンにし、cpp と c に command_c の命令（既定は前後に空白を足しただけの同じ clangd）を書いた
// 設定。more は（名前・命令）を足す。
[[nodiscard]] SettingsReading
share_settings(std::string command_c = " " + std::string{clangd} + "\t",
               std::vector<std::pair<std::string, std::string>> more = {})
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"c", std::move(command_c)}, {"cpp", std::string{clangd}}};
    for (auto &entry : more)
    {
        settings.lsp_commands.push_back(std::move(entry));
    }
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

void hold_all(Editing &editing)
{
    for (const std::string_view path :
         {cpp_path, c_path, other_c_path, ts_path, js_path, python_path, rust_path})
    {
        editing.files().hold_at(std::string(path), Bytes{std::string("x\n")});
    }
}

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

// 席 slot の起きた子を ServerStarted → `initialize` の答え → `initialized` まで進める。
void run_slot(Editing &editing, Slot slot)
{
    auto &server = editing.language_server();
    server.serve(slot, app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    server.answer(slot, initialize_id(server, slot), running_answer);
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// path を開いて root を探した答えの合図を 1 回送る（子が起きたならその席を running まで進める）。
void opened(Editing &editing, std::string_view path)
{
    const std::size_t before = editing.language_server().launches().size();
    applied(editing.controller(), app::OpenDocument{path_of(path)});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    if (editing.language_server().launches().size() > before)
    {
        run_slot(editing, editing.language_server().started_slots().back());
    }
}

[[nodiscard]] std::size_t count_method(const std::vector<std::string> &bodies,
                                       std::string_view method)
{
    const std::string key = R"("method":")" + std::string{method} + "\"";
    return static_cast<std::size_t>(std::ranges::count_if(bodies, [&key](const std::string &body)
                                                          { return contains(body, key); }));
}

// 席 slot へ送った `didOpen` のうち languageId が id の数。
[[nodiscard]] std::size_t opened_as(const ScriptedLanguageServer &server, Slot slot,
                                    std::string_view id)
{
    const std::string key = R"("languageId":")" + std::string{id} + "\"";
    return static_cast<std::size_t>(std::ranges::count_if(
        server.sent_to(slot), [&key](const std::string &body)
        { return contains(body, R"("method":"textDocument/didOpen")") && contains(body, key); }));
}

// (a) 同じ命令の行（前後の空白だけ違う）と同じ root の c と cpp は子が 1 本で、didOpen は 2 つとも
// 同じ子に languageId 別で届く。
void verify_shared_server()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    expect(server.launches().size() == 1 && server.started_slots() == std::vector{Slot::first},
           "c and cpp with the same command line and root share one server");
    expect(opened_as(server, Slot::first, "cpp") == 1 && opened_as(server, Slot::first, "c") == 1,
           "both documents are opened on the shared server with their own languageId");
    applied(editing.controller(), app::InsertText{"c"});
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"p"});
    expect(count_method(server.sent_to(Slot::first), "textDocument/didChange") == 2 &&
               server.stops() == 0,
           "edits of both languages go to the shared server");
}

// (b) root が違えば 2 本・(c) 命令の行が違えば 2 本。
void verify_separate_keys()
{
    Editing far(share_settings());
    hold_all(far);
    opened(far, cpp_path);
    opened(far, other_c_path);
    expect(far.language_server().launches().size() == 2 &&
               far.language_server().launches().back().root.text() == "C:\\other",
           "a c document under another root starts its own server");
    Editing flagged(share_settings(std::string{clangd} + " --log=error"));
    hold_all(flagged);
    opened(flagged, cpp_path);
    opened(flagged, c_path);
    expect(flagged.language_server().launches().size() == 2 &&
               flagged.language_server().started_slots().back() == Slot::second &&
               opened_as(flagged.language_server(), Slot::second, "c") == 1,
           "a different command line is a different server");
}

// (d)
// 共有の子が落ちたら同じ席で起こし直し、両方の言語の文書がアクティブになったときに開き直される。
void verify_shared_restart()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    server.serve(Slot::first, app::ServerExited{1});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(server.launches().size() == 2 && server.started_slots().back() == Slot::first,
           "the shared server is restarted once in the same slot");
    run_slot(editing, Slot::first);
    applied(editing.controller(), app::SwitchTab{0});
    expect(opened_as(server, Slot::first, "c") == 2 && opened_as(server, Slot::first, "cpp") == 2 &&
               server.launches().size() == 2,
           "after the restart the documents of both languages are opened again on it");
}

// (e) 失敗と given_up は席（命令の行）ごと。共有の子を諦めると c も cpp も起こさず、ほかの命令の
// 行の言語は起きる。`:set lsp` で戻る。
void verify_shared_give_up()
{
    Editing editing(
        share_settings(" " + std::string{clangd}, {{"python", R"(C:\Python\jedi.exe)"}}));
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    for (std::size_t round = 0; round < 4; ++round)
    {
        server.serve(Slot::first, app::ServerExited{1});
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    }
    expect(server.launches().size() == 4, "the shared server is restarted three times");
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::SwitchTab{1});
    expect(server.launches().size() == 4,
           "neither language of a given-up shared server starts it again");
    opened(editing, python_path);
    expect(server.launches().size() == 5 &&
               server.launches().back().executable.text() == "C:\\Python\\jedi.exe",
           "a language with another command line still starts");
    applied(editing.controller(), app::SwitchTab{0});
    static_cast<void>(run_ex(editing.controller(), "set lsp"));
    expect(server.launches().size() == 6 && server.launches().back().executable.text() == clangd,
           "set lsp clears the given-up state of the shared command line");
}

// (f) 片方の言語の文書を全部閉じても、もう片方が残っていれば子は生きる。
void verify_close_one_language()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    applied(editing.controller(), app::CloseTab{1});
    applied(editing.controller(), app::InsertText{"p"});
    expect(count_method(server.sent_to(Slot::first), "textDocument/didClose") == 1 &&
               count_method(server.sent(), "shutdown") == 0 && server.stops() == 0 &&
               count_method(server.sent_to(Slot::first), "textDocument/didChange") == 1,
           "closing every c document keeps the shared server for the cpp document");
}

// `:set ft=c` を cpp の文書に掛けると、同じ席のまま didClose して languageId c で開き直す。
void verify_shared_filetype_change()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    auto &server = editing.language_server();
    static_cast<void>(run_ex(editing.controller(), "set ft=c"));
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(count_method(server.sent_to(Slot::first), "textDocument/didClose") == 1 &&
               opened_as(server, Slot::first, "c") == 1 && server.launches().size() == 1,
           "a filetype changed between sharing languages reopens on the same server");
}

// (g) 席の上限は子の数で数える。c・cpp・typescript・javascript・python・rust の 6 言語は 4 本で
// 足り、どの席も譲らない。
void verify_slot_count()
{
    constexpr std::string_view tsserver =
        R"("C:\node\node.exe" C:\npm\typescript-language-server\lib\cli.mjs --stdio)";
    Editing editing(share_settings(std::string{clangd}, {{"javascript", std::string{tsserver}},
                                                         {"python", R"(C:\Python\jedi.exe)"},
                                                         {"rust", R"(C:\Rust\ra.exe)"},
                                                         {"typescript", std::string{tsserver}}}));
    hold_all(editing);
    for (const std::string_view path : {cpp_path, c_path, ts_path, js_path, python_path, rust_path})
    {
        opened(editing, path);
    }
    auto &server = editing.language_server();
    expect(server.launches().size() == 4 && count_method(server.sent(), "shutdown") == 0 &&
               server.stops() == 0,
           "six languages over four command lines fill four slots without yielding");
    expect(opened_as(server, Slot::second, "typescript") == 1 &&
               opened_as(server, Slot::second, "javascript") == 1,
           "typescript and javascript share the second slot");
}
} // namespace

void verify_language_server_shares()
{
    verify_shared_server();
    verify_separate_keys();
    verify_shared_restart();
    verify_shared_give_up();
    verify_close_one_language();
    verify_shared_filetype_change();
    verify_slot_count();
}
} // namespace nenenib::tests
