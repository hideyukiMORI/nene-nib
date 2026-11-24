// 契約 --language-server の席の共有の部分（ADR 0160 / 0161 の
// 2026-11-24 の追補・#1952）。席の鍵は（命令の行・root）で、
// c と cpp のように同じ命令の行の言語は子を 1 本だけ起こして共有する。
// `didOpen` の languageId は文書ごと。失敗と given_up は席の鍵
// （命令の行・root）ごと。共有の子が落ちたら両方の言語の文書が
// 開き直される。片方の言語の文書を閉じても子は生きる。席の上限は
// 子の数で数える。設定の命令の行の変更（#2007 のレビュー R1）・
// 共有しない言語への `:set ft=`（R2）・要求と診断の宛先と正規化の
// 境と知らせの名前（R5）も縛る。替え玉 ScriptedLanguageServer で
// 確かめる。本物のサーバーも窓も使わない。
#include "ChangeSetting.hpp"
#include "CloseSettings.hpp"
#include "CloseTab.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FoundLanguageServer.hpp"
#include "InsertText.hpp"
#include "JumpSettingsSection.hpp"
#include "KnownLanguageServers.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageServerSlot.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "QueryLanguage.hpp"
#include "RevertSettings.hpp"
#include "Scopes.hpp"
#include "ServerExited.hpp"
#include "ServerStarted.hpp"
#include "SettingsSection.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
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

// (d) 共有の子が落ちたら同じ席で起こし直し、両方の言語の文書が
// アクティブになったときに開き直される。
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

// (e) 失敗と given_up は席の鍵（命令の行・root）ごと。共有の子を
// 諦めると c も cpp も起こさず、ほかの命令の行の言語は起きる。
// `:set lsp` で戻る。
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

constexpr std::string_view one_error =
    R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"severity":1,"message":"bad"}])";
constexpr std::string_view c_uri = "file:///C:/work/util.c";
constexpr std::string_view cpp_uri = "file:///C:/work/main.cpp";

// 席 slot へ送った `shutdown` の最後の id（無ければ 0）。
[[nodiscard]] std::int64_t shutdown_id(const ScriptedLanguageServer &server, Slot slot)
{
    std::int64_t id = 0;
    for (const std::string &body : server.sent_to(slot))
    {
        constexpr std::string_view key = R"("id":)";
        if (contains(body, R"("method":"shutdown")"))
        {
            id = std::stoll(body.substr(body.find(key) + key.size()));
        }
    }
    return id;
}

// 席 slot が待っている `shutdown` に答える（答えの後の reconcile が次を起こす）。
void answer_shutdown(Editing &editing, Slot slot)
{
    auto &server = editing.language_server();
    server.answer(slot, shutdown_id(server, slot), "null");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// M1: 同じ命令で root の違う 2 席は失敗の控えを分ける。片方を諦めても
// もう片方は止めず、その root の文書は受け続ける。
void verify_give_up_by_root()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, other_c_path);
    auto &server = editing.language_server();
    for (std::size_t round = 0; round < 4; ++round)
    {
        server.serve(Slot::first, app::ServerExited{1});
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    }
    applied(editing.controller(), app::InsertText{"o"});
    expect(server.launches().size() == 5 &&
               std::ranges::all_of(server.stopped_slots(),
                                   [](Slot slot) { return slot == Slot::first; }) &&
               count_method(server.sent_to(Slot::second), "textDocument/didChange") == 1,
           "giving up the server of one root keeps the same command under another root");
    applied(editing.controller(), app::SwitchTab{0});
    expect(server.launches().size() == 5, "the given-up root is not started by switching");
    server.serve(Slot::second, app::ServerExited{1});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(server.launches().size() == 6 && server.started_slots().back() == Slot::second,
           "the same command under the other root still restarts after its own failure");
}

// M1: 片方の席の `initialized` は、root の違う同じ命令の席の続けた
// 失敗の数を戻さない（交互に落ちても上限で止まる）。
void verify_initialized_by_root()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, other_c_path);
    auto &server = editing.language_server();
    const auto fail = [&editing, &server](Slot slot)
    {
        server.serve(slot, app::ServerExited{1});
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    };
    fail(Slot::first);
    fail(Slot::first);
    fail(Slot::second);
    run_slot(editing, Slot::second);
    fail(Slot::first);
    fail(Slot::first);
    expect(server.launches().size() == 6,
           "the other root reaching initialized does not reset this root's failures");
}

// 探しの結果の替え玉（表の行 0 の候補 0 の clangd だけが見つかった）。
[[nodiscard]] ScriptedLanguageServerSearch::Found found_clangd()
{
    std::size_t count = 0;
    for (const auto &row : core::known_language_servers())
    {
        count += row.candidates.size();
    }
    ScriptedLanguageServerSearch::Found found(count);
    found.at(0) = app::FoundLanguageServer{path_of(clangd), std::nullopt};
    return found;
}

// 設定の面を開いて探しの結果を届け、「言語サーバー」の節の最初の行（C / C++）に焦点を置く。
void opened_panel(Editing &editing)
{
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    editing.language_server_search().serve(found_clangd());
    static_cast<void>(controller.apply(app::WorkCompleted{}));
    static_cast<void>(
        controller.apply(app::JumpSettingsSection{core::SettingsSection::language_server}));
}

// R1 の 1: 共有の命令の行のうち c だけを変えると、前の行の席を止める
// （cpp が前の行を使い続けていても・ADR の限界）。c は新しい行の席で、
// cpp はアクティブにしたときに前の行の席で開く。面の「外す」「使う」
// の後の「元に戻す」で c だけが変わる形を作る。
void verify_one_language_command_change()
{
    const std::string flagged = std::string{clangd} + " --x";
    Editing editing(share_settings(flagged));
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    opened_panel(editing);
    static_cast<void>(editing.controller().apply(app::ChangeSetting{true}));
    static_cast<void>(editing.controller().apply(app::ChangeSetting{true}));
    // 「使う」の直後に空いた席（third）で clangd の席が起きる。
    const Slot shared = server.started_slots().back();
    answer_shutdown(editing, Slot::first);
    answer_shutdown(editing, Slot::second);
    run_slot(editing, shared);
    expect(server.launches().size() == 3 && server.launches().back().arguments.empty() &&
               shared == Slot::third && opened_as(server, shared, "c") == 1,
           "Use writes the same clangd for c and cpp and the c document joins its server");
    const std::size_t closes = count_method(server.sent_to(shared), "textDocument/didClose");
    static_cast<void>(editing.controller().apply(app::RevertSettings{}));
    expect(count_method(server.sent_to(shared), "shutdown") == 1 &&
               count_method(server.sent_to(shared), "textDocument/didClose") == closes + 1,
           "changing only c's line shuts the server of its previous line");
    answer_shutdown(editing, shared);
    expect(server.launches().size() == 4 &&
               server.launches().back().arguments == std::vector<std::string>{"--x"},
           "the c document starts a server of its new line");
    static_cast<void>(editing.controller().apply(app::CloseSettings{}));
    run_slot(editing, server.started_slots().back());
    applied(editing.controller(), app::SwitchTab{0});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(server.launches().size() == 5 && server.launches().back().arguments.empty(),
           "the cpp document starts the server of the line it still has");
}

// R1 の 3: 諦めた命令の行を面で外して足し直すと、控えが消えてもう一度起きる。
void verify_given_up_line_readded()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    for (std::size_t round = 0; round < 4; ++round)
    {
        server.serve(Slot::first, app::ServerExited{1});
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    }
    opened_panel(editing);
    static_cast<void>(editing.controller().apply(app::ChangeSetting{true}));
    expect(server.launches().size() == 4, "removing the given-up line starts nothing");
    static_cast<void>(editing.controller().apply(app::ChangeSetting{true}));
    expect(server.launches().size() == 5 && server.launches().back().executable.text() == clangd,
           "adding the line back clears its given-up state and starts it again");
}

// R1 の 4 と R3: 前後の空白だけを変えた命令の行は同じ命令で、席を止めない。
void verify_blank_only_change_keeps_server()
{
    Editing editing(share_settings(" " + std::string{clangd} + " "));
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    opened_panel(editing);
    static_cast<void>(editing.controller().apply(app::ChangeSetting{true}));
    static_cast<void>(editing.controller().apply(app::ChangeSetting{true}));
    const Slot shared = server.started_slots().back();
    answer_shutdown(editing, Slot::first);
    run_slot(editing, shared);
    const std::size_t launches = server.launches().size();
    const std::size_t closes = count_method(server.sent_to(shared), "textDocument/didClose");
    static_cast<void>(editing.controller().apply(app::RevertSettings{}));
    expect(shared != Slot::first && count_method(server.sent_to(shared), "shutdown") == 0 &&
               count_method(server.sent_to(shared), "textDocument/didClose") == closes &&
               server.launches().size() == launches && launches == 2,
           "a line changed only by blanks around it keeps the running server");
}

// R2: 共有の席から別の命令の言語・命令の無い言語へ `:set ft=` で移ると、
// 共有の席へ didClose するだけで止めず、もう片方の言語の文書は受け続ける。
void verify_filetype_leaves_shared()
{
    Editing editing(share_settings(std::string{clangd}, {{"python", R"(C:\Python\jedi.exe)"}}));
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    static_cast<void>(run_ex(editing.controller(), "set ft=python"));
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    run_slot(editing, Slot::second);
    expect(count_method(server.sent_to(Slot::first), "textDocument/didClose") == 1 &&
               server.stops() == 0 && opened_as(server, Slot::second, "python") == 1,
           "a c document turned python leaves the shared server and opens on python's");
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"p"});
    expect(count_method(server.sent_to(Slot::first), "textDocument/didChange") == 1,
           "the cpp document keeps the shared server");
    static_cast<void>(run_ex(editing.controller(), "set ft=text"));
    applied(editing.controller(), app::InsertText{"t"});
    expect(count_method(server.sent_to(Slot::first), "textDocument/didClose") == 2 &&
               server.stops() == 0 && server.launches().size() == 2 &&
               count_method(server.sent(), "shutdown") == 0,
           "a plain filetype closes the document and keeps the server without starting one");
}

// R5 の 1〜3: 共有の席の上で、席を起こしていない言語（c）の文書の要求が
// 同じ席へ出て答えが置かれ、診断は URI の文書へ置かれ、子が落ちると
// 両方のタブの診断が消える。
void verify_shared_requests_and_diagnostics()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    auto &server = editing.language_server();
    applied(editing.controller(), app::QueryLanguage{core::LanguageQueryKind::hover});
    const auto asked = server.requests("textDocument/hover");
    expect(asked.size() == 1 &&
               count_method(server.sent_to(Slot::first), "textDocument/hover") == 1,
           "a hover of the c document is asked of the server cpp started");
    server.answer(Slot::first, asked.back(), R"({"contents":"int c"})");
    const auto answered = editing.controller().apply_frame(app::WorkCompleted{});
    expect(answered.hover == std::optional<std::string>{"int c"},
           "the answer is placed on the c tab");
    server.publish(Slot::first, c_uri, std::nullopt, one_error);
    server.publish(Slot::first, cpp_uri, std::nullopt, one_error);
    const auto c_tab = editing.controller().apply_frame(app::WorkCompleted{});
    const auto cpp_tab = editing.controller().apply_frame(app::SwitchTab{0});
    expect(c_tab.diagnostic_counts.errors == 1 && cpp_tab.diagnostic_counts.errors == 1,
           "diagnostics of the shared server land on the tab of their URI");
    server.serve(Slot::first, app::ServerExited{1});
    const auto ended = editing.controller().apply_frame(app::WorkCompleted{});
    const auto other = editing.controller().apply_frame(app::SwitchTab{1});
    expect(ended.diagnostic_counts.errors == 0 && other.diagnostic_counts.errors == 0,
           "the shared server ending clears the diagnostics of both tabs");
}

// R5 の 4: 賢く畳まない。大小だけ・引用符の有無だけ・間の空白の数だけ
// 違う命令の行は別の命令で、子が 2 本起きる。
void verify_command_key_not_folded()
{
    for (const std::string &variant :
         {std::string{R"(C:\LLVM\bin\CLANGD.EXE)"}, "\"" + std::string{clangd} + "\"",
          std::string{clangd} + "  --x"})
    {
        const std::string cpp_line =
            variant.ends_with("--x") ? std::string{clangd} + " --x" : std::string{clangd};
        auto settings = core::default_editor_settings();
        settings.lsp = true;
        settings.lsp_commands = {{"c", variant}, {"cpp", cpp_line}};
        Editing editing(SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}});
        hold_all(editing);
        opened(editing, cpp_path);
        opened(editing, c_path);
        expect(editing.language_server().launches().size() == 2,
               "case, quotes and inner blanks make a different command line");
    }
}

// R5 の 5: cpp が起こした共有の席が c のタブで落ちると、知らせの名前は c。
void verify_notice_names_document_language()
{
    Editing editing(share_settings());
    hold_all(editing);
    opened(editing, cpp_path);
    opened(editing, c_path);
    editing.language_server().serve(Slot::first, app::ServerExited{1});
    const auto ended = editing.controller().apply_frame(app::WorkCompleted{});
    const std::string expected =
        core::ui_text_filled(core::UiMessage::notice_language_server_ended, ended.language, {"c"});
    expect(ended.command_message.has_value() && ended.command_message.value().text() == expected,
           "the notice of the shared server names the language of the active document");
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
    verify_give_up_by_root();
    verify_initialized_by_root();
    verify_one_language_command_change();
    verify_given_up_line_readded();
    verify_blank_only_change_keeps_server();
    verify_filetype_leaves_shared();
    verify_shared_requests_and_diagnostics();
    verify_command_key_not_folded();
    verify_notice_names_document_language();
}
} // namespace nenenib::tests
