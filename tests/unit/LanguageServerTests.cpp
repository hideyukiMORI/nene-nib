// 契約 --language-server（ADR 0160 の決定 1・5〜7・9・追補・Issue #994）。替え玉
// ScriptedLanguageServer で、起動 → `initialize` → 答え → `initialized`・失敗の値・再起動の
// 上限 3・tick の古さ・設定の on / off で start / stop・`:set lsp` の要求を確かめる。本物の
// clangd も窓も使わない。
#include "CloseTab.hpp"
#include "Column.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DiagnosticCounts.hpp"
#include "DiagnosticSeverity.hpp"
#include "DiagnosticSpan.hpp"
#include "DiagnosticsCount.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "ExLspRequest.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "GridCellSpan.hpp"
#include "GridDiagnosticSpan.hpp"
#include "GuideContext.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerTick.hpp"
#include "LineView.hpp"
#include "LspDiagnostics.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerCommand.hpp"
#include "ServerEvent.hpp"
#include "ServerExited.hpp"
#include "ServerFailed.hpp"
#include "ServerFailure.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "StatusItems.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

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
    expect(server.sent().size() == 3 &&
               contains(server.sent().at(1), R"("method":"initialized")") &&
               !contains(server.sent().at(1), R"("id")") &&
               contains(last_sent(editing), R"("method":"textDocument/didOpen")"),
           "the answer to initialize is followed by initialized and then the open document");
    expect(contains(server.sent().front(), R"("rootUri":"file:///C:/work")"),
           "initialize names the launch folder as the root URI (ADR 0161 decision 1)");
    const std::size_t before = server.touches();
    applied(editing.controller(), app::InsertText{"x"});
    applied(editing.controller(), app::InsertText{"y"});
    expect(server.touches() == before + 2 &&
               server.notifications("textDocument/didChange").size() == 2,
           "each typed intent sends exactly one didChange (ADR 0161 decision 1)");
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
    const auto &sent = server.sent();
    expect(contains(last_sent(editing), R"("method":"shutdown")") &&
               contains(sent.at(sent.size() - 2), R"("method":"textDocument/didClose")"),
           "a document without a command closes the open documents and shuts the server down");
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

// 最後に送った要求に result で答える（`"id":N` の N を本文から読む）。
void answered(Editing &editing)
{
    const std::string &sent = last_sent(editing);
    constexpr std::string_view key = R"("id":)";
    const std::size_t begin = sent.find(key) + key.size();
    const std::string id = sent.substr(begin, sent.find_first_of(",}", begin) - begin);
    static_cast<void>(delivered(
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" + id + R"(,"result":{}})"}));
}

// 失敗の数は続けて失敗した回数で、`initialized` まで届いたら 0 に戻る（決定 6・R1-4）。
void verify_failures_reset()
{
    Editing editing(lsp_settings(true));
    started(editing);
    auto &server = editing.language_server();
    for (std::size_t round = 0; round < 5; ++round)
    {
        static_cast<void>(delivered(editing, app::ServerExited{1}));
        static_cast<void>(delivered(editing, app::ServerStarted{}));
        answered(editing);
    }
    const auto &sent = server.sent();
    expect(server.launches().size() == 6 &&
               contains(sent.at(sent.size() - 2), R"("method":"initialized")") &&
               contains(last_sent(editing), R"("method":"textDocument/didOpen")"),
           "a server that initializes again is restarted after every single failure");
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

// ---------------------------------------------------------------- 段 L1（ADR 0161・#1210）

constexpr std::string_view source_uri = "file:///C:/work/main.cpp";
constexpr std::string_view second_path = "C:\\work\\two.cpp";
constexpr std::string_view second_uri = "file:///C:/work/two.cpp";
constexpr std::string_view script_path = "C:\\work\\run.py";
// main.cpp の `main`（0 行目の 4〜8 文字目）の error。文は 2 行目を捨てて 1 行になる。
constexpr std::string_view main_error =
    R"([{"range":{"start":{"line":0,"character":4},"end":{"line":0,"character":8}},"severity":1,"message":"bad\nmore"}])";
// main.cpp の `int`（0 行目の 0〜3 文字目）の warning。
constexpr std::string_view int_warning =
    R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":3}},"severity":2,"message":"w"}])";
constexpr std::size_t shown_lines = 4;

// 本文の `"version":N` の N（無ければ値なし）。
[[nodiscard]] std::optional<std::uint64_t> version_in(std::string_view body)
{
    constexpr std::string_view key = R"("version":)";
    const std::size_t at = body.find(key);
    if (at == std::string_view::npos)
    {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (std::size_t index = at + key.size();
         index < body.size() && body[index] >= '0' && body[index] <= '9'; ++index)
    {
        value = value * 10 + static_cast<std::uint64_t>(body[index] - '0');
    }
    return value;
}

[[nodiscard]] std::size_t occurrences(std::string_view text, std::string_view part)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string_view::npos;
         at = text.find(part, at + part.size()))
    {
        ++count;
    }
    return count;
}

// 行の下線がちょうど 1 つの区間 [begin, end)（桁は 1 始まり）か。
[[nodiscard]] bool underlined(const app::LineView &line, std::size_t begin, std::size_t end,
                              core::DiagnosticSeverity severity)
{
    return line.diagnostics ==
           std::vector{core::DiagnosticSpan{core::Column{begin}, core::Column{end}, severity}};
}

// 見えている行を持つ設定で起こす（started と同じ形に VisibleLines を足す）。
void started_with_lines(Editing &editing)
{
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
    started(editing);
}

// lsp を on にし、cpp と python に命令を書いた設定。
[[nodiscard]] SettingsReading two_language_settings()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}, {"python", "pylsp"}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

// 決定 1: `didOpen` は on・命令のある言語・4 MiB 以下の文書を `initialized` の後に全文で送る。
void verify_did_open()
{
    Editing editing(lsp_settings(true));
    started(editing);
    auto &server = editing.language_server();
    const auto opened = server.notifications("textDocument/didOpen");
    expect(opened.size() == 1 && contains(opened.front(), R"("uri":"file:///C:/work/main.cpp")") &&
               contains(opened.front(), R"("languageId":"cpp")") &&
               version_in(opened.front()) == std::optional<std::uint64_t>{0} &&
               contains(opened.front(), R"("text":"int main() {}\n")"),
           "the active C++ document is opened with its URI, language, revision and full text");

    Editing big(lsp_settings(true));
    big.files().hold_at(std::string(source_path),
                        Bytes{std::string(app::language_server_document_limit + 1, 'x')});
    applied(big.controller(), app::OpenDocument{path_of(source_path)});
    static_cast<void>(delivered(big, app::ServerStarted{}));
    answered(big);
    expect(big.language_server().notifications("textDocument/didOpen").empty() &&
               contains(big.language_server().sent().back(), R"("method":"initialized")"),
           "a document over 4 MiB is not sent to the server");

    Editing growing(lsp_settings(true));
    started(growing);
    applied(growing.controller(),
            app::InsertText{std::string(app::language_server_document_limit, 'x')});
    expect(growing.language_server().notifications("textDocument/didClose").size() == 1 &&
               growing.language_server().notifications("textDocument/didChange").empty(),
           "an edit that grows the document over 4 MiB closes it instead of sending the change");
}

// 決定 1: 1 意図の編集は 1 通の `didChange` に順のまま束ね、version は意図の後の revision()。
void verify_did_change_bundles()
{
    Editing editing(lsp_settings(true));
    started(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::InsertText{"x"});
    const auto typed = server.notifications("textDocument/didChange");
    expect(
        typed.size() == 1 && version_in(typed.front()) == std::optional<std::uint64_t>{1} &&
            contains(
                typed.front(),
                R"("contentChanges":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":0}},"text":"x"}])"),
        "one typed character is one incremental change at the caret with the next version");

    Editing macro(lsp_settings(true));
    macro.files().hold_at(std::string(second_path), Bytes{std::string("abc\ndef\nghi\n")});
    applied(macro.controller(), app::SelectEditMode{core::EditMode::vim});
    applied(macro.controller(), app::OpenDocument{path_of(second_path)});
    static_cast<void>(delivered(macro, app::ServerStarted{}));
    answered(macro);
    for (const auto &key : vim_keys_of("qaxjxq"))
    {
        applied(macro.controller(), app::VimKeyPress{key});
    }
    const auto recorded = macro.language_server().notifications("textDocument/didChange");
    for (const auto &key : vim_keys_of("@a"))
    {
        applied(macro.controller(), app::VimKeyPress{key});
    }
    const auto replayed = macro.language_server().notifications("textDocument/didChange");
    expect(
        recorded.size() == 2 && replayed.size() == 3 &&
            occurrences(replayed.back(), R"("range")") == 2 &&
            contains(
                replayed.back(),
                R"({"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}},"text":""},{"range":{"start":{"line":2,"character":0},"end":{"line":2,"character":1}},"text":""})") &&
            version_in(replayed.back()).value_or(0) > version_in(recorded.back()).value_or(0),
        "a macro's two edits go out as one didChange in order with a later version");

    Editing undo(lsp_settings(true));
    started(undo);
    applied(undo.controller(), app::InsertText{"x"});
    applied(undo.controller(), app::HistoryAction{core::HistoryDirection::undo});
    const auto undone = undo.language_server().notifications("textDocument/didChange");
    expect(
        undone.size() == 2 &&
            contains(
                undone.back(),
                R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"text":""})") &&
            version_in(undone.back()) == std::optional<std::uint64_t>{2},
        "undo sends the reverse edit with a new version");
}

// 決定 1（R3-1）: 意図の中の編集が差し引き 0 なら版は進まないので、didChange を送らない。
void verify_net_zero_intent()
{
    Editing macro(lsp_settings(true));
    macro.files().hold_at(std::string(second_path), Bytes{std::string("abc\n")});
    applied(macro.controller(), app::SelectEditMode{core::EditMode::vim});
    applied(macro.controller(), app::OpenDocument{path_of(second_path)});
    static_cast<void>(delivered(macro, app::ServerStarted{}));
    answered(macro);
    auto &server = macro.language_server();
    for (const auto &key : vim_keys_of("qaix<Esc>xq"))
    {
        applied(macro.controller(), app::VimKeyPress{key});
    }
    const auto recorded = server.notifications("textDocument/didChange");
    for (const auto &key : vim_keys_of("@a"))
    {
        applied(macro.controller(), app::VimKeyPress{key});
    }
    expect(!recorded.empty() &&
               server.notifications("textDocument/didChange").size() == recorded.size(),
           "a replay that types and deletes the same character sends no didChange");
    for (const auto &key : vim_keys_of("x"))
    {
        applied(macro.controller(), app::VimKeyPress{key});
    }
    const auto after = server.notifications("textDocument/didChange");
    expect(after.size() == recorded.size() + 1 &&
               version_in(after.back()).value_or(0) > version_in(recorded.back()).value_or(0),
           "the next real edit still goes out with a version above the last sent one");
}

// 決定 1: タブを閉じたら `didClose`。切り替えと保存では送らない（決定 8）。2 言語目は前を閉じる。
void verify_did_close()
{
    Editing editing(lsp_settings(true));
    hold_documents(editing);
    editing.files().hold_at(std::string(second_path), Bytes{std::string("abc\n")});
    started(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(second_path)});
    expect(server.notifications("textDocument/didOpen").size() == 2 &&
               contains(server.notifications("textDocument/didOpen").back(),
                        R"("uri":"file:///C:/work/two.cpp")"),
           "a second document of the same language is opened on the same server");
    const std::size_t quiet = server.sent().size();
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::SwitchTab{1});
    static_cast<void>(editing.controller().apply_frame(
        app::SaveDocument{path_of(second_path), core::TextEncoding::utf8}));
    expect(server.sent().size() == quiet && server.launches().size() == 1,
           "switching between open documents and saving send nothing (no didSave)");
    applied(editing.controller(), app::CloseTab{1});
    const auto closed = server.notifications("textDocument/didClose");
    expect(closed.size() == 1 && contains(closed.front(), second_uri),
           "closing a tab closes its document on the server");
    static_cast<void>(run_ex(editing.controller(), "set nolsp"));
    const auto &sent = server.sent();
    expect(server.notifications("textDocument/didClose").size() == 2 &&
               contains(sent.at(sent.size() - 2), R"("uri":"file:///C:/work/main.cpp")") &&
               contains(sent.back(), R"("method":"shutdown")"),
           "set nolsp closes the remaining documents before shutdown");

    Editing languages(two_language_settings());
    hold_documents(languages);
    languages.files().hold_at(std::string(script_path), Bytes{std::string("x = 1\n")});
    started(languages);
    applied(languages.controller(), app::OpenDocument{path_of(script_path)});
    const auto &order = languages.language_server().sent();
    expect(contains(order.back(), R"("method":"shutdown")") &&
               contains(order.at(order.size() - 2), R"("method":"textDocument/didClose")") &&
               contains(order.at(order.size() - 2), source_uri),
           "a document of a second language closes the first language's documents and server");
}

// 決定 1（R5-1）: 名前を付けて保存で URI が変われば、古い URI を閉じて新しい URI を今の版で開き、
// 古い URI の診断は片付ける。
void verify_saved_as()
{
    constexpr std::string_view renamed_path = "C:\\work\\renamed.cpp";
    Editing editing(lsp_settings(true));
    started_with_lines(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::InsertText{"x"});
    server.publish(source_uri, 1, main_error);
    const auto marked = editing.controller().apply_frame(app::WorkCompleted{});
    const std::size_t before = server.sent().size();
    const auto saved = editing.controller().apply_frame(
        app::SaveDocument{path_of(renamed_path), core::TextEncoding::utf8});
    const auto &sent = server.sent();
    expect(!marked.lines.at(0).diagnostics.empty() && sent.size() == before + 2 &&
               contains(sent.at(before), R"("method":"textDocument/didClose")") &&
               contains(sent.at(before), source_uri) &&
               contains(sent.back(), R"("method":"textDocument/didOpen")") &&
               contains(sent.back(), R"("uri":"file:///C:/work/renamed.cpp")") &&
               version_in(sent.back()) == std::optional<std::uint64_t>{1},
           "saving under another name closes the old URI and opens the new one at the revision");
    expect(saved.lines.at(0).diagnostics.empty(),
           "the old URI's diagnostics are cleared when it is closed");
}

// 決定 2・5: `publishDiagnostics` は文書の値に載り、見えている行の LineView.diagnostics に写る。
void verify_diagnostics_placed()
{
    Editing editing(lsp_settings(true));
    started_with_lines(editing);
    auto &server = editing.language_server();
    server.publish(source_uri, 0, main_error);
    const auto frame = editing.controller().apply_frame(app::WorkCompleted{});
    expect(frame.lines.size() == 2 &&
               underlined(frame.lines.at(0), 5, 9, core::DiagnosticSeverity::error) &&
               frame.lines.at(1).diagnostics.empty(),
           "a published error is underlined on its visible line only");
    server.publish(source_uri, 0, "[]");
    expect(editing.controller().apply_frame(app::WorkCompleted{}).lines.at(0).diagnostics.empty(),
           "an empty publish clears the document's diagnostics");
    server.publish("file:///C:/work/other.cpp", 0, main_error);
    server.publish(source_uri, 99, main_error);
    expect(editing.controller().apply_frame(app::WorkCompleted{}).lines.at(0).diagnostics.empty(),
           "diagnostics for an unopened URI or an unsent version are dropped");

    const core::TextViewGeometry geometry{85, 20, 96};
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines, geometry}));
    server.publish(source_uri, 0, main_error);
    static_cast<void>(editing.controller().apply(app::WorkCompleted{}));
    const auto grid = editing.controller().apply_frame(app::SelectDisplay{core::DisplayForm::grid});
    expect(!grid.grid.empty() && grid.grid.front().diagnostics ==
                                     std::vector{app::GridDiagnosticSpan{
                                         app::GridCellSpan{4, 8}, core::DiagnosticSeverity::error}},
           "the grid row carries the same underline in cells");
}

// 決定 3: 編集で診断をずらし、古い version の診断は届いた版の本文で写してから後の編集でずらす。
void verify_diagnostics_follow_edits()
{
    Editing editing(lsp_settings(true));
    started_with_lines(editing);
    auto &server = editing.language_server();
    server.publish(source_uri, 0, main_error);
    static_cast<void>(editing.controller().apply(app::WorkCompleted{}));
    const auto typed = editing.controller().apply_frame(app::InsertText{"xy"});
    expect(underlined(typed.lines.at(0), 7, 11, core::DiagnosticSeverity::error),
           "typing before a diagnostic moves it with the text");
    server.publish(source_uri, 0, int_warning);
    const auto stale = editing.controller().apply_frame(app::WorkCompleted{});
    expect(underlined(stale.lines.at(0), 3, 6, core::DiagnosticSeverity::warning),
           "a diagnostic for an older version is placed through the later edits");
    const auto before =
        editing.controller().apply_frame(app::DeleteText{core::DeleteDirection::backward});
    expect(underlined(before.lines.at(0), 2, 5, core::DiagnosticSeverity::warning),
           "deleting before a diagnostic moves it back");
    const auto inside =
        editing.controller().apply_frame(app::DeleteText{core::DeleteDirection::forward});
    expect(inside.lines.at(0).diagnostics.empty(),
           "an edit inside the range drops the diagnostic until the server publishes again");
}

// 決定 2・5: 脇に置いた文書の診断も、その束に載って切り替えで見える。
void verify_parked_diagnostics()
{
    Editing editing(lsp_settings(true));
    editing.files().hold_at(std::string(second_path), Bytes{std::string("abc\n")});
    started_with_lines(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(second_path)});
    server.publish(source_uri, 0, main_error);
    const auto aside = editing.controller().apply_frame(app::WorkCompleted{});
    expect(aside.lines.at(0).diagnostics.empty(),
           "a parked document's diagnostics do not show on the active document");
    const auto back = editing.controller().apply_frame(app::SwitchTab{0});
    expect(underlined(back.lines.at(0), 5, 9, core::DiagnosticSeverity::error),
           "switching back shows the diagnostics placed while it was parked");
    static_cast<void>(run_ex(editing.controller(), "set nolsp"));
    expect(editing.controller().frame().lines.at(0).diagnostics.empty(),
           "closing the documents on the server clears their diagnostics");
}
// 本文の診断の文（値なしは "<none>"）。
[[nodiscard]] std::string diagnostic_message(const app::EditorFrame &frame)
{
    return frame.diagnostic_message.has_value()
               ? std::string(frame.diagnostic_message.value().text())
               : "<none>";
}

// キャレットを 1 行目の桁 column（1 始まり）へ置いた frame。
[[nodiscard]] app::EditorFrame caret_at(Editing &editing, std::size_t column)
{
    return editing.controller().apply_frame(
        app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{column}},
                        core::SelectionAnchoring::collapse});
}

// main.cpp の 1 行目に error（`main`）・warning（`int`）・information（`(`）を置いて届かせる。
void diagnosed(Editing &editing)
{
    started_with_lines(editing);
    editing.language_server().publish(
        source_uri, 0,
        R"([{"range":{"start":{"line":0,"character":4},"end":{"line":0,"character":8}},"severity":1,"message":"bad\nmore"},)"
        R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":3}},"severity":2,"message":"w"},)"
        R"({"range":{"start":{"line":0,"character":8},"end":{"line":0,"character":9}},"severity":3,"message":"i"}])");
}

// 決定 4（R7-1・L1c）: 件数とキャレットの上の文が frame に載る。文はキャレットが区間の外なら無く、
// 文を出す frame は案内を消す。
void verify_diagnostic_frame()
{
    Editing editing(lsp_settings(true));
    started_with_lines(editing);
    const auto quiet = editing.controller().frame();
    expect(quiet.diagnostic_counts ==
                   core::DiagnosticCounts{0, 0, core::DiagnosticsCount::complete} &&
               !quiet.diagnostic_message.has_value(),
           "a document without diagnostics has no count and no message");
    Editing marked(lsp_settings(true));
    diagnosed(marked);
    const auto first = marked.controller().apply_frame(app::WorkCompleted{});
    expect(first.diagnostic_counts ==
               core::DiagnosticCounts{1, 1, core::DiagnosticsCount::complete},
           "the frame counts errors and warnings but not information");
    expect(diagnostic_message(first) == "w" && first.guide.context == core::GuideContext::hidden,
           "the caret on the warning shows its message and hides the guide");
    expect(diagnostic_message(caret_at(marked, 6)) == "bad" &&
               diagnostic_message(caret_at(marked, 9)) == "i",
           "the message follows the caret onto the error and the information");
    expect(diagnostic_message(caret_at(marked, 12)) == "<none>",
           "outside every range there is no message");
    expect(marked.controller().diagnostic_counts() == first.diagnostic_counts,
           "the window's count reads the same value as the frame");
}

// 決定 5（L1c）: showMessage の error / warning だけ 1 行の知らせで、診断の文より勝つ。Info・Log と
// logMessage は知らせを残さない。
void verify_show_message_notices()
{
    Editing editing(lsp_settings(true));
    diagnosed(editing);
    auto &server = editing.language_server();
    static_cast<void>(editing.controller().apply(app::WorkCompleted{}));
    server.serve(app::ServerMessage{
        R"({"jsonrpc":"2.0","method":"window/showMessage","params":{"type":1,"message":"boom"}})"});
    const auto told = editing.controller().apply_frame(app::WorkCompleted{});
    expect(message(told) == "boom" && diagnostic_message(told) == "<none>",
           "an error showMessage is a notice and wins over the caret's diagnostic message");
    static_cast<void>(caret_at(editing, 1));
    server.serve(app::ServerMessage{
        R"({"jsonrpc":"2.0","method":"window/showMessage","params":{"type":2,"message":"careful"}})"});
    expect(message(editing.controller().apply_frame(app::WorkCompleted{})) == "careful",
           "a warning showMessage is a notice");
    static_cast<void>(caret_at(editing, 12));
    server.serve(app::ServerMessage{
        R"({"jsonrpc":"2.0","method":"window/showMessage","params":{"type":3,"message":"info"}})"});
    server.serve(app::ServerMessage{
        R"({"jsonrpc":"2.0","method":"window/showMessage","params":{"type":4,"message":"log"}})"});
    server.serve(app::ServerMessage{
        R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"type":1,"message":"log"}})"});
    expect(message(editing.controller().apply_frame(app::WorkCompleted{})).empty(),
           "info and log showMessage and logMessage leave no notice");
}

// 決定 2・4（L1c）: 上限を超えた診断は残した件数に `+` を付けて数える。
void verify_truncated_count()
{
    Editing editing(lsp_settings(true));
    started_with_lines(editing);
    std::string many = "[";
    for (std::size_t index = 0; index <= core::lsp_diagnostics_limit; ++index)
    {
        many += index == 0 ? "" : ",";
        many +=
            R"({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"severity":1,"message":"e"})";
    }
    editing.language_server().publish(source_uri, 0, many + "]");
    const auto truncated = editing.controller().apply_frame(app::WorkCompleted{});
    const auto item = core::diagnostic_count_item(truncated.diagnostic_counts);
    expect(item.has_value() && item.value().text() == "E1000+",
           "a list over the limit counts the kept errors and ends with +");
}

// 決定 4（R7-1・L1c）: 行末の空の診断は、本文も格子も行末の外の 1 桁（1 升）に下線を引く。
void verify_line_end_diagnostic()
{
    Editing editing(lsp_settings(true));
    started_with_lines(editing);
    auto &server = editing.language_server();
    constexpr std::string_view at_end =
        R"([{"range":{"start":{"line":0,"character":13},"end":{"line":0,"character":13}},"severity":1,"message":"expected ';'"}])";
    server.publish(source_uri, 0, at_end);
    const auto text = editing.controller().apply_frame(app::WorkCompleted{});
    expect(underlined(text.lines.at(0), 14, 15, core::DiagnosticSeverity::error),
           "a text line underlines one column past the line end");
    const core::TextViewGeometry geometry{400, 20, 96};
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines, geometry}));
    const auto grid = editing.controller().apply_frame(app::SelectDisplay{core::DisplayForm::grid});
    expect(!grid.grid.empty() &&
               grid.grid.front().diagnostics ==
                   std::vector{app::GridDiagnosticSpan{app::GridCellSpan{13, 14},
                                                       core::DiagnosticSeverity::error}},
           "a grid row underlines the one cell past the line end");
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
    verify_failures_reset();
    verify_start_refused();
    verify_staleness();
    verify_tick_is_not_an_operation();
    verify_quiet_when_off();
    verify_did_open();
    verify_did_change_bundles();
    verify_net_zero_intent();
    verify_did_close();
    verify_saved_as();
    verify_diagnostics_placed();
    verify_diagnostics_follow_edits();
    verify_parked_diagnostics();
    verify_diagnostic_frame();
    verify_show_message_notices();
    verify_truncated_count();
    verify_line_end_diagnostic();
}

void verify_language_server_scope()
{
    verify_language_server_contracts();
}
} // namespace nenenib::tests
