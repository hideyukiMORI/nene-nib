// 契約 --language-server（ADR 0160 の決定 1・5〜7・9・追補・Issue #994）。替え玉
// ScriptedLanguageServer で、起動 → `initialize` → 答え → `initialized`・失敗の値・再起動の
// 上限 3・tick の古さ・設定の on / off で start / stop・`:set lsp` の要求を確かめる。本物の
// clangd も窓も使わない。
#include "CancelCommand.hpp"
#include "CloseCompletion.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "CommandText.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DiagnosticCounts.hpp"
#include "DiagnosticSeverity.hpp"
#include "DiagnosticSpan.hpp"
#include "DiagnosticsCount.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ExLspRequest.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "GridCellSpan.hpp"
#include "GridDiagnosticSpan.hpp"
#include "GuideContext.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "HoverTip.hpp"
#include "InsertText.hpp"
#include "KeyChord.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerTick.hpp"
#include "LayoutRect.hpp"
#include "LineView.hpp"
#include "LspDiagnostics.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCompletion.hpp"
#include "OpenContextMenu.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "PlaceCaret.hpp"
#include "QueryLanguage.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
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
#include "Session.hpp"
#include "SessionTab.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "StatusItems.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TabTipMeasure.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimAction.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimKeyTable.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WindowDeactivated.hpp"
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

// frame のキャレットが line 行 column 桁（1 始まり）か。
[[nodiscard]] bool landed_at(const app::EditorFrame &frame, std::size_t line, std::size_t column)
{
    return frame.caret.position == core::TextPosition{core::LineNumber{line}, core::Column{column}};
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
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2}}})"}));
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
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","textDocumentSync":2}}})"}));
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
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" + id +
                                    R"(,"result":{"capabilities":{"textDocumentSync":2}}})"}));
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
    server.refuse(app::ServerFailure::cannot_start);
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
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2}}})"}));
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

// 決定 1 と ADR 0168 の追補 段 2a（レビュー V-1666 の M2）: `:s///c` の問いは意図をまたぎ、答えの
// たびに didChange を送る。終わりの畳みが改訂番号を問いの途中より小さく戻すと、畳んだ後の最初の
// 編集の版が送り済みの版以下になって捨てられ、サーバーの本文がずれる。
void verify_question_fold_versions()
{
    Editing editing(lsp_settings(true));
    editing.files().hold_at(std::string(second_path), Bytes{std::string("a\na\na\n")});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    applied(editing.controller(), app::OpenDocument{path_of(second_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    answered(editing);
    auto &server = editing.language_server();
    // 入力行が開いている間の VimKeyPress は捨てられるので、`:` の行は :normal! の口で打つ。
    vim_normal(editing.controller(), ":%s/a/X/gc<CR>");
    for (const std::string_view answer : {"y", "y", "q"})
    {
        applied(editing.controller(), app::CommandText{std::string(answer)});
    }
    const auto answered_changes = server.notifications("textDocument/didChange");
    for (const auto &key : vim_keys_of("x"))
    {
        applied(editing.controller(), app::VimKeyPress{key});
    }
    const auto after = server.notifications("textDocument/didChange");
    expect(
        answered_changes.size() == 2 && after.size() == 3 &&
            version_in(after.back()).value_or(0) >
                version_in(answered_changes.back()).value_or(0) &&
            contains(
                after.back(),
                R"({"range":{"start":{"line":2,"character":0},"end":{"line":2,"character":1}},"text":""})"),
        "the first edit after the question's fold goes out with a version above every answer's");
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
           "switching between open documents and saving send nothing (no save ability)");
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
    const auto before = editing.controller().apply_frame(
        app::DeleteText{core::DeleteDirection::backward, nenenib::core::DeleteUnit::character});
    expect(underlined(before.lines.at(0), 2, 5, core::DiagnosticSeverity::warning),
           "deleting before a diagnostic moves it back");
    const auto inside = editing.controller().apply_frame(
        app::DeleteText{core::DeleteDirection::forward, nenenib::core::DeleteUnit::character});
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

// 決定 4 の追補（R3-3）: マクロの録画中は診断の文を出さず `recording @a` を隠さない。窓の口
// diagnostic_message_shown は frame の文の有無と同じ値。
void verify_recording_hides_message()
{
    Editing editing(lsp_settings(true));
    diagnosed(editing);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    const auto shown = editing.controller().apply_frame(app::WorkCompleted{});
    expect(diagnostic_message(shown) == "w" && editing.controller().diagnostic_message_shown(),
           "the message shows in Vim mode and the window's reading agrees");
    for (const auto &key : vim_keys_of("qa"))
    {
        applied(editing.controller(), app::VimKeyPress{key});
    }
    const auto recording = editing.controller().frame();
    expect(recording.recording.has_value() && diagnostic_message(recording) == "<none>" &&
               !editing.controller().diagnostic_message_shown(),
           "recording a macro hides the diagnostic message so `recording @a` stays visible");
    for (const auto &key : vim_keys_of("q"))
    {
        applied(editing.controller(), app::VimKeyPress{key});
    }
    expect(diagnostic_message(editing.controller().frame()) == "w",
           "the message comes back when the recording stops");
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

// ---------------------------------------------- 段 L2 / L3 の土台（ADR 0190・#1825）

// 補完（triggerCharacters の数は捨てる）・hover（true）・定義へ（object）を持つサーバーの答え。
constexpr std::string_view capable_answer =
    R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","textDocumentSync":2,"completionProvider":{"triggerCharacters":[".","->",3]},"hoverProvider":true,"definitionProvider":{}}}})";
constexpr std::string_view main_location =
    R"({"uri":"file:///C:/work/main.cpp","range":{"start":{"line":0,"character":4},"end":{"line":0,"character":8}}})";

// 開いて、能力つきで `initialize` に答える（started の能力つきの形）。
void capable(Editing &editing)
{
    hold_documents(editing);
    editing.files().hold_at(std::string(second_path), Bytes{std::string("abc\n")});
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(editing, app::ServerMessage{std::string{capable_answer}}));
}

// 最後に送った method の要求の id。
[[nodiscard]] std::int64_t last_request(Editing &editing, std::string_view method)
{
    const auto ids = editing.language_server().requests(method);
    return ids.empty() ? -1 : ids.back();
}

// 置いた hover の平文（置いていなければ "<none>"・空の答えは "<empty>"）。
[[nodiscard]] std::string hover_text(Editing &editing)
{
    const auto &hover = editing.controller().language_answers().hover;
    return hover.has_value() ? hover.value().text.value_or("<empty>") : "<none>";
}

// 答えを仕込み、合図の意図で引かせる。
void answer_with(Editing &editing, std::int64_t id, std::string_view result)
{
    editing.language_server().answer(id, result);
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// 決定 3: client の能力を申し出、答えの能力を持つ。能力の無いサーバーには要求を送らない。
void verify_abilities()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    const auto &server = editing.language_server();
    expect(
        contains(
            server.sent().front(),
            R"("textDocument":{"completion":{"completionItem":{"snippetSupport":false,"documentationFormat":["plaintext"]}},"hover":{"contentFormat":["plaintext","markdown"]},"definition":{"linkSupport":true}})"),
        "initialize offers completion without snippets, hover text and definition links");
    const auto &abilities = editing.controller().language_server_abilities();
    expect(abilities.completion && abilities.hover && abilities.definition &&
               abilities.trigger_characters == std::vector<std::string>{".", "->"},
           "the answered capabilities keep the providers and the text trigger characters");

    Editing plain(lsp_settings(true));
    started(plain);
    const std::size_t before = plain.language_server().sent().size();
    expect(!plain.controller().request_completion(1) && !plain.controller().request_hover() &&
               !plain.controller().request_definition() &&
               plain.language_server().sent().size() == before,
           "a server without the providers is sent no requests");

    Editing stopped(lsp_settings(true));
    capable(stopped);
    static_cast<void>(run_ex(stopped.controller(), "set nolsp"));
    static_cast<void>(
        delivered(stopped, app::ServerMessage{R"({"jsonrpc":"2.0","id":2,"result":null})"}));
    expect(!stopped.controller().language_server_abilities().hover &&
               stopped.controller().language_server_abilities().trigger_characters.empty(),
           "a stopped server keeps no abilities");
}

// 決定 2: `didOpen` していない文書（起動中・無題）では要求しない。
void verify_requests_need_an_open_document()
{
    Editing editing(lsp_settings(true));
    hold_documents(editing);
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    expect(!editing.controller().request_hover(), "nothing is requested before initialized");
    static_cast<void>(delivered(editing, app::ServerMessage{std::string{capable_answer}}));
    expect(editing.controller().request_hover(), "an open C++ document can ask for a hover");
    const auto &server = editing.language_server();
    applied(editing.controller(), app::NewTab{});
    const std::size_t untitled = server.sent().size();
    expect(!editing.controller().request_hover() && !editing.controller().request_definition() &&
               !editing.controller().request_completion(1) && server.sent().size() == untitled,
           "an untitled document sends no request");
}

// 決定 1・2: 要求は文書の URI とキャレットの Position で、変更の `didChange` の後に送る。
void verify_request_params()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    applied(editing.controller(), app::InsertText{"x"});
    expect(editing.controller().request_definition(), "the definition request is sent");
    const auto &sent = server.sent();
    expect(
        contains(sent.back(), R"("method":"textDocument/definition")") &&
            contains(
                sent.back(),
                R"("params":{"textDocument":{"uri":"file:///C:/work/main.cpp"},"position":{"line":0,"character":1}})") &&
            contains(sent.at(sent.size() - 2), R"("method":"textDocument/didChange")"),
        "the request names the document and the caret after the change it follows");
    expect(editing.controller().request_completion(4), "the completion request is sent");
    expect(contains(server.sent().back(), R"("context":{"triggerKind":1})"),
           "a completion opened by hand is Invoked");
    expect(editing.controller().request_completion(5, std::string_view{"."}),
           "a completion from a trigger character is sent");
    expect(contains(server.sent().back(), R"("context":{"triggerKind":2,"triggerCharacter":"."})"),
           "a trigger character is told to the server");
}

// 決定 1: hover と定義への答えは同じ文書・同じ版・同じキャレットのときだけ使う。
void verify_answer_context()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(editing.controller().request_hover());
    answer_with(editing, last_request(editing, "textDocument/hover"), R"({"contents":"doc"})");
    const auto &answers = editing.controller().language_answers();
    expect(hover_text(editing) == "doc" && answers.hover.has_value() &&
               answers.hover.value().context.at == core::Offset{0},
           "a current hover answer is placed with its context");
    static_cast<void>(editing.controller().request_hover());
    const std::int64_t typed = last_request(editing, "textDocument/hover");
    applied(editing.controller(), app::InsertText{"y"});
    answer_with(editing, typed, R"({"contents":"typed"})");
    expect(hover_text(editing) == "<none>",
           "a hover answer for an older revision is dropped and clears the earlier answer");
    static_cast<void>(editing.controller().request_hover());
    answer_with(editing, last_request(editing, "textDocument/hover"), R"({"contents":"doc"})");
    static_cast<void>(editing.controller().request_hover());
    const std::int64_t moved = last_request(editing, "textDocument/hover");
    static_cast<void>(caret_at(editing, 4));
    answer_with(editing, moved, R"({"contents":"moved"})");
    expect(hover_text(editing) == "<none>",
           "a hover answer for another caret is dropped and clears the earlier answer");
    static_cast<void>(editing.controller().request_hover());
    answer_with(editing, last_request(editing, "textDocument/hover"), "null");
    expect(hover_text(editing) == "<none>",
           "an empty hover answer is told and not kept (the tip stage drops it)");
    applied(editing.controller(), app::OpenDocument{path_of(second_path)});
    static_cast<void>(editing.controller().request_definition());
    const std::int64_t elsewhere = last_request(editing, "textDocument/definition");
    applied(editing.controller(), app::SwitchTab{0});
    answer_with(editing, elsewhere, std::string{"["} + std::string{main_location} + "]");
    expect(!editing.controller().language_answers().definition.has_value(),
           "a definition answer for another tab is dropped");
    static_cast<void>(editing.controller().request_definition());
    answer_with(editing, last_request(editing, "textDocument/definition"), main_location);
    expect(!editing.controller().language_answers().definition.has_value() &&
               landed_at(editing.controller().frame(), 1, 5),
           "a current definition answer is used once (the caret lands on it)");
}

// 決定 1: 同じ種類の新しい要求は古い要求を取り消し、遅れた古い答えは捨てる。ほかの種類は
// 取り消さない。
void verify_cancel()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(editing.controller().request_definition());
    const std::int64_t first = last_request(editing, "textDocument/definition");
    static_cast<void>(editing.controller().request_hover());
    expect(server.cancelled().empty(), "a request of another kind cancels nothing");
    static_cast<void>(editing.controller().request_definition());
    const std::int64_t second = last_request(editing, "textDocument/definition");
    expect(server.cancelled() == std::vector<std::int64_t>{first} && second != first,
           "a new definition request cancels the older one");
    answer_with(editing, first, main_location);
    expect(!editing.controller().language_answers().definition.has_value(),
           "the late answer to the cancelled request is dropped");
    editing.language_server().answer(second, "null");
    expect(message(editing.controller().apply_frame(app::WorkCompleted{})) ==
               "定義が見つかりません",
           "the answer to the newer request is used (no definition is told)");
}

// 決定 1: 補完の答えは同じ文書がアクティブなときに、面の通し番号と共に置く。誤りの答えは空。
void verify_completion_answer()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(editing.controller().request_completion(7));
    const std::int64_t old = last_request(editing, "textDocument/completion");
    static_cast<void>(editing.controller().request_completion(8, std::string_view{"."}));
    expect(server.cancelled() == std::vector<std::int64_t>{old},
           "a new completion request cancels the older one");
    answer_with(editing, last_request(editing, "textDocument/completion"),
                R"({"isIncomplete":true,"items":[{"label":"main"},{"label":"max"}]})");
    const auto session = [&editing]
    {
        const auto &completion = editing.controller().language_answers().completion;
        return completion.has_value() ? completion.value().session : 0;
    };
    // 置いた候補の数と isIncomplete（置いていなければ値なし）。
    const auto placed = [&editing]
    {
        const auto &completion = editing.controller().language_answers().completion;
        return completion.has_value()
                   ? std::optional{std::pair{completion.value().list.items.size(),
                                             completion.value().list.incomplete}}
                   : std::nullopt;
    };
    expect(session() == 8 && placed() == std::optional{std::pair{std::size_t{2}, true}},
           "a completion answer is placed with its session");
    static_cast<void>(editing.controller().request_completion(9));
    server.serve(
        app::ServerMessage{R"({"jsonrpc":"2.0","id":)" +
                           std::to_string(last_request(editing, "textDocument/completion")) +
                           R"(,"error":{"code":-32603,"message":"x"}})"});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(session() == 9 && placed() == std::optional{std::pair{std::size_t{0}, false}},
           "an error answer is an empty list");
    applied(editing.controller(), app::OpenDocument{path_of(second_path)});
    static_cast<void>(editing.controller().request_completion(10));
    const std::int64_t elsewhere = last_request(editing, "textDocument/completion");
    applied(editing.controller(), app::SwitchTab{0});
    answer_with(editing, elsewhere, R"([{"label":"x"}])");
    expect(session() == 0,
           "a completion answer for another tab is dropped and clears the last one");
    static_cast<void>(editing.controller().request_completion(11));
    const std::int64_t cancelled = last_request(editing, "textDocument/completion");
    static_cast<void>(editing.controller().request_completion(12));
    answer_with(editing, cancelled, R"([{"label":"old"}])");
    expect(session() == 0, "the answer to a cancelled completion is not placed");
    answer_with(editing, last_request(editing, "textDocument/completion"), R"([{"label":"new"}])");
    expect(session() == 12, "the answer carries the session of the request it answers");
}

// 能力の欄を capabilities（JSON の object）にした `initialize` の答えで、開いて進める。
void started_with(Editing &editing, std::string_view capabilities)
{
    hold_documents(editing);
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    editing.language_server().answer(last_request(editing, "initialize"),
                                     std::string{R"({"capabilities":)"} +
                                         std::string{capabilities} + "}");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// 決定 3: 能力ごとに出し分ける（持たない能力の要求だけを送らない）。
void verify_abilities_one_by_one()
{
    Editing completing(lsp_settings(true));
    started_with(completing, R"({"completionProvider":{},"hoverProvider":false})");
    expect(completing.controller().request_completion(1) &&
               !completing.controller().request_hover() &&
               !completing.controller().request_definition() &&
               completing.controller().language_server_abilities().trigger_characters.empty(),
           "a server with completion only is asked for completion only");
    Editing hovering(lsp_settings(true));
    started_with(hovering,
                 R"({"hoverProvider":{"workDoneProgress":true},"definitionProvider":false})");
    expect(!hovering.controller().request_completion(1) && hovering.controller().request_hover() &&
               !hovering.controller().request_definition(),
           "hover options count as the hover provider and false is no provider");
}

// 決定 2: 言語の違う文書・4 MiB を超えた文書・shutting_down の間は要求しない。
void verify_requests_need_a_sent_document()
{
    constexpr std::string_view big_path = "C:\\work\\big.cpp";
    Editing editing(lsp_settings(true));
    editing.files().hold_at(std::string(big_path),
                            Bytes{std::string(app::language_server_document_limit + 1, 'x')});
    capable(editing);
    applied(editing.controller(), app::OpenDocument{path_of(big_path)});
    expect(!editing.controller().request_hover() && !editing.controller().request_completion(1),
           "a document over 4 MiB is not sent and asks nothing");
    applied(editing.controller(), app::SwitchTab{0});
    expect(editing.controller().request_hover(), "the sent C++ document still asks");
    applied(editing.controller(), app::OpenDocument{path_of(note_path)});
    const std::size_t sent = editing.language_server().sent().size();
    expect(contains(last_sent(editing), R"("method":"shutdown")") &&
               !editing.controller().request_hover() &&
               !editing.controller().request_definition() &&
               editing.language_server().sent().size() == sent,
           "a document of another language shuts the server down and asks nothing meanwhile");
    applied(editing.controller(), app::SwitchTab{0});
    expect(!editing.controller().request_hover(),
           "the C++ document asks nothing while the server is shutting down");
}

// 決定 1: 落ちた・止めた子の保留の要求と答えは残らず、起こし直した子の id は前と重ならない。
void verify_answers_after_restart()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(editing.controller().request_hover());
    answer_with(editing, last_request(editing, "textDocument/hover"), R"({"contents":"doc"})");
    static_cast<void>(editing.controller().request_definition());
    const std::int64_t pending = last_request(editing, "textDocument/definition");
    static_cast<void>(delivered(editing, app::ServerExited{1}));
    expect(hover_text(editing) == "<none>" &&
               !editing.controller().language_server_abilities().hover &&
               editing.language_server().launches().size() == 2,
           "an exited server leaves no answers and no abilities and is started again");
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    const std::int64_t initialize = last_request(editing, "initialize");
    expect(initialize > pending, "the restarted server's ids continue after the old ones");
    editing.language_server().answer(
        initialize, R"({"capabilities":{"hoverProvider":true,"definitionProvider":true}})");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    expect(editing.controller().language_server_abilities().hover &&
               !editing.controller().language_server_abilities().completion,
           "the restarted server's answer sets the abilities again");
    answer_with(editing, pending, main_location);
    expect(!editing.controller().language_answers().definition.has_value(),
           "an answer to the old server's request is dropped");
    static_cast<void>(editing.controller().request_hover());
    answer_with(editing, last_request(editing, "textDocument/hover"), R"({"contents":"again"})");
    expect(hover_text(editing) == "again", "the restarted server answers a new request");
    static_cast<void>(run_ex(editing.controller(), "set nolsp"));
    expect(!editing.controller().request_hover(), "nothing is asked while shutting down");
    answer_with(editing, last_request(editing, "shutdown"), "null");
    expect(hover_text(editing) == "<none>", "a stopped server leaves no answers");
}

// 決定 1: 答えない要求は 2 回の tick で `$/cancelRequest` を送って捨てる（知らせず・止めない）。
void verify_request_expiry()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(editing.controller().request_hover());
    const std::int64_t id = last_request(editing, "textDocument/hover");
    static_cast<void>(editing.controller().apply_frame(app::LanguageServerTick{}));
    const auto stale = editing.controller().apply_frame(app::LanguageServerTick{});
    expect(server.cancelled() == std::vector<std::int64_t>{id} && message(stale).empty() &&
               server.stops() == 0,
           "a silent hover request is cancelled after two ticks without a notice");
    answer_with(editing, id, R"({"contents":"late"})");
    expect(hover_text(editing) == "<none>", "the answer after the expiry is dropped");
}

// ---------------------------------------------- 段 L3: hover の札と定義へ（ADR 0190 の決定
// 6〜8・#1834）

constexpr std::string_view two_location =
    R"({"uri":"file:///C:/work/two.cpp","range":{"start":{"line":0,"character":2},"end":{"line":0,"character":3}}})";

// 能力つきで起こし、Vim のモードにする。
void capable_vim(Editing &editing)
{
    capable(editing);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
}

// 答えを仕込み、合図の意図で引かせた frame。
[[nodiscard]] app::EditorFrame answered_frame(Editing &editing, std::int64_t id,
                                              std::string_view result)
{
    editing.language_server().answer(id, result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

// `gd` を打って、その定義の要求に result で答えた frame。
[[nodiscard]] app::EditorFrame defined(Editing &editing, std::string_view result)
{
    vim_replay(editing.controller(), "gd");
    return answered_frame(editing, last_request(editing, "textDocument/definition"), result);
}

// `K` を打って、その hover の要求に result で答えた frame。
[[nodiscard]] app::EditorFrame hovered(Editing &editing, std::string_view result)
{
    vim_replay(editing.controller(), "K");
    return answered_frame(editing, last_request(editing, "textDocument/hover"), result);
}

[[nodiscard]] std::string active_path(Editing &editing)
{
    const auto frame = editing.controller().frame();
    const auto &path = frame.tabs.at(frame.active_tab).path;
    return path.has_value() ? std::string(path.value().text()) : "";
}

// 決定 8: 表の行（core）。`K` は通常の表、`gd` は g の表、F12 は両モードの「定義へ移動」。
void verify_query_tables()
{
    expect(core::vim_action_for(U'K') == core::VimAction::language_hover &&
               core::vim_g_action_for(U'd') == core::VimAction::language_definition &&
               core::vim_language_query_for(core::VimAction::language_hover) ==
                   core::LanguageQueryKind::hover &&
               core::vim_language_query_for(core::VimAction::language_definition) ==
                   core::LanguageQueryKind::definition &&
               !core::vim_language_query_for(core::VimAction::move_left).has_value(),
           "K and gd are rows of the key tables that name a language query");
    const core::KeyChord f12{false, false, core::OperationKey::f12};
    expect(core::operation_for(f12, core::EditMode::ordinary) ==
                   core::EditorOperation::go_to_definition &&
               core::operation_for(f12, core::EditMode::vim) ==
                   core::EditorOperation::go_to_definition &&
               !core::shown_chord(core::EditorOperation::show_information, core::EditMode::ordinary)
                    .has_value(),
           "F12 goes to the definition in both modes and show information has no key");
}

// 決定 8: `gd` `K` は NORMAL で回数もオペレータも無いときだけ問う。
void verify_vim_keys_ask()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto &server = editing.language_server();
    vim_replay(editing.controller(), "K");
    expect(server.requests("textDocument/hover").size() == 1, "K asks for a hover");
    vim_replay(editing.controller(), "gd");
    expect(server.requests("textDocument/definition").size() == 1, "gd asks for the definition");
    const std::size_t before = server.sent().size();
    vim_replay(editing.controller(), "3K2gddK<Esc>dgd<Esc>vK<Esc>vgd<Esc>");
    expect(server.sent().size() == before &&
               whole_vim_body(editing.controller()) == "int main() {}\n",
           "a count, a pending operator and VISUAL ask nothing and change nothing");
}

// 決定 8: 言語サーバーが動いていない文書では、表に行が無い鍵（`Q` と `gx`）と同じ失敗に見える。
void verify_vim_keys_fail_without_server()
{
    Editing with_k;
    Editing with_q;
    for (Editing *editing : {&with_k, &with_q})
    {
        open_vim_document(*editing, "abc def\n");
    }
    // :normal! の意味では失敗した鍵の後ろを流さない。打った鍵の意味では後ろの鍵が走る。
    vim_normal(with_k.controller(), "Kx");
    vim_normal(with_q.controller(), "Qx");
    expect(whole_vim_body(with_k.controller()) == "abc def\n" &&
               whole_vim_body(with_q.controller()) == "abc def\n",
           "K fails like an unbound key and ends a :normal! run");
    vim_normal(with_k.controller(), "gdx");
    vim_normal(with_q.controller(), "gxx");
    expect(whole_vim_body(with_k.controller()) == "abc def\n" &&
               whole_vim_body(with_q.controller()) == "abc def\n",
           "gd fails like an unbound g key and ends a :normal! run");
    vim_replay(with_k.controller(), "3Kx");
    vim_replay(with_q.controller(), "3Qx");
    const auto k_frame = with_k.controller().frame();
    expect(whole_vim_body(with_k.controller()) == whole_vim_body(with_q.controller()) &&
               whole_vim_body(with_k.controller()) == "bc def\n" && message(k_frame).empty() &&
               with_k.language_server().touches() == 0,
           "a typed K drops the count like an unbound key, says nothing and touches no server");
}

// 決定 7: 同じ文書の定義は `''` を置いてキャレットを移す。答えは使ったら消える。
void verify_definition_in_document()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto frame = defined(editing, main_location);
    expect(landed_at(frame, 1, 5) && message(frame).empty() &&
               !editing.controller().language_answers().definition.has_value(),
           "gd lands on the definition in the same document and uses the answer once");
    vim_replay(editing.controller(), "``");
    expect(landed_at(editing.controller().frame(), 1, 1),
           "the place before the jump is kept in the '' mark");
}

// 決定 7: 別のファイルは開いて着地する（全文検索の当たりと同じ道）。行が本文の外なら末尾。
void verify_definition_in_other_file()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto frame = defined(editing, two_location);
    expect(active_path(editing) == second_path && landed_at(frame, 1, 3),
           "gd opens the other file and lands on the definition");
    Editing plain(lsp_settings(true));
    capable(plain);
    static_cast<void>(
        plain.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition}));
    const auto beyond = answered_frame(
        plain, last_request(plain, "textDocument/definition"),
        R"({"uri":"file:///C:/work/two.cpp","range":{"start":{"line":9,"character":0},"end":{"line":9,"character":1}}})");
    expect(active_path(plain) == second_path && landed_at(beyond, 2, 1),
           "a line outside the opened text lands at its end");
}

// 決定 7: 2 つ以上は最初へ跳んで「1 / N」・無ければ 1 行・file: 以外は 1 行・開けなければ動かない。
void verify_definition_notices()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto first =
        defined(editing, "[" + std::string{main_location} + "," + std::string{two_location} + "]");
    expect(landed_at(first, 1, 5) && message(first) == "定義 1 / 2",
           "several definitions land on the first and tell how many there are");
    const auto none = defined(editing, "[]");
    expect(landed_at(none, 1, 5) && message(none) == "定義が見つかりません",
           "no definition is told in one line");
    const auto null = defined(editing, "null");
    expect(message(null) == "定義が見つかりません", "a null answer is no definition");
    const auto foreign = defined(
        editing,
        R"({"uri":"jdt://contents/x.class","range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})");
    expect(landed_at(foreign, 1, 5) &&
               message(foreign) == "定義の場所はファイルではないので開けません",
           "a location that is not a file is told in one line");
    const auto missing = defined(
        editing,
        R"({"uri":"file:///C:/work/missing.cpp","range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}}})");
    expect(active_path(editing) == source_path && landed_at(missing, 1, 5) &&
               !message(missing).empty(),
           "a definition in a file that cannot be opened leaves the caret and tells why");
}

// 決定 1: 文脈の違う答え（キャレットが動いた・入力行が開いた）では跳ばない・知らせない。
void verify_definition_context()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    vim_replay(editing.controller(), "gd");
    const std::int64_t asked = last_request(editing, "textDocument/definition");
    vim_replay(editing.controller(), "l");
    const auto moved = answered_frame(editing, asked, two_location);
    expect(active_path(editing) == source_path && landed_at(moved, 1, 2) && message(moved).empty(),
           "a definition answer for another caret does nothing");
    vim_replay(editing.controller(), "gd");
    const std::int64_t again = last_request(editing, "textDocument/definition");
    vim_replay(editing.controller(), ":");
    static_cast<void>(answered_frame(editing, again, two_location));
    expect(active_path(editing) == source_path &&
               !editing.controller().language_answers().definition.has_value(),
           "a definition answer that arrives while a command line is open is dropped");
}

// 決定 6: `K` の答えは frame の札に載る。合図では消えず、使う人の操作と窓の非アクティブ化で消える。
void verify_hover_tip_shown()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto shown = hovered(editing, R"({"contents":"int main"})");
    expect(shown.hover == std::optional<std::string>{"int main"} && message(shown).empty(),
           "the hover answer is carried by the frame");
    expect(editing.controller().apply_frame(app::WorkCompleted{}).hover.has_value() &&
               editing.controller().apply_frame(app::AutosaveTick{}).hover.has_value() &&
               editing.controller().apply_frame(app::VisibleLines{shown_lines}).hover.has_value(),
           "signals and the window size keep the tip");
    expect(!editing.controller().apply_frame(app::WindowDeactivated{}).hover.has_value() &&
               !editing.controller().language_answers().hover.has_value(),
           "a deactivated window drops the tip and the answer");
    const std::vector<app::EditorIntent> operations{
        app::VimKeyPress{core::VimKey{core::VimSpecialKey::escape}},
        app::ScrollLines{1},
        app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{1}},
                        core::SelectionAnchoring::collapse},
        app::NewTab{},
    };
    bool dropped = true;
    for (const app::EditorIntent &operation : operations)
    {
        applied(editing.controller(), app::SwitchTab{0});
        dropped = dropped && hovered(editing, R"({"contents":"doc"})").hover.has_value() &&
                  !editing.controller().apply_frame(operation).hover.has_value();
    }
    expect(dropped, "a key, the wheel, a click and a tab switch drop the tip");
}

// 決定 6: 空の答えは札を出さず 1 行知らせる。文脈の違う答えは何も起こさない。12 行で切る。
void verify_hover_tip_edges()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto empty = hovered(editing, "null");
    expect(!empty.hover.has_value() && message(empty) == "情報がありません" &&
               !editing.controller().language_answers().hover.has_value(),
           "an empty hover answer shows no tip and tells so in one line");
    vim_replay(editing.controller(), "K");
    const std::int64_t asked = last_request(editing, "textDocument/hover");
    vim_replay(editing.controller(), "l");
    const auto moved = answered_frame(editing, asked, R"({"contents":"late"})");
    expect(!moved.hover.has_value() && message(moved).empty(),
           "a hover answer for another caret shows nothing and tells nothing");
    const auto tall =
        hovered(editing, R"({"contents":"1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13"})");
    expect(tall.hover == std::optional<std::string>{"1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12…"},
           "a hover of more than twelve lines is cut with an ellipsis");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::ordinary});
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::hover}));
    const auto edited = answered_frame(editing, last_request(editing, "textDocument/hover"),
                                       R"({"contents":"doc"})");
    expect(edited.hover.has_value() &&
               !editing.controller().apply_frame(app::InsertText{"x"}).hover.has_value(),
           "an edit drops the tip");
}

// 決定 8・5: 操作（F12・情報を表示）は通常モードでも問い、言語サーバーが動いていなければ知らせる。
void verify_query_operations()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    const auto &server = editing.language_server();
    const auto asked =
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition});
    expect(server.requests("textDocument/definition").size() == 1 && message(asked).empty(),
           "the go to definition operation asks in the ordinary mode");
    const auto landed =
        answered_frame(editing, last_request(editing, "textDocument/definition"), main_location);
    expect(landed_at(landed, 1, 5), "the operation's answer lands like gd");
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::hover}));
    const auto tip = answered_frame(editing, last_request(editing, "textDocument/hover"),
                                    R"({"contents":"doc"})");
    expect(tip.hover == std::optional<std::string>{"doc"},
           "the show information operation shows the tip");

    Editing off;
    hold_documents(off);
    applied(off.controller(), app::OpenDocument{path_of(source_path)});
    const auto quiet =
        off.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition});
    const auto silent =
        off.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::hover});
    expect(message(quiet) == "言語サーバーが動いていません" &&
               message(silent) == "言語サーバーが動いていません" &&
               off.language_server().touches() == 0,
           "without a running server both operations tell so in one line");
}

// 決定 6: 札の文の切り詰めと置き場（core の純関数）。
void verify_hover_tip_core()
{
    expect(core::hover_tip_text("a\r\nb\rc\n\n") == "a\nb\nc" &&
               core::hover_tip_text("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12") ==
                   "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12" &&
               core::hover_tip_text("1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n \n13") ==
                   "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11…",
           "the tip text uses line feeds, drops trailing blank lines and keeps twelve lines");
    expect(core::hover_tip_clipped("abc def", 7) == "abc def" &&
               core::hover_tip_clipped("abc def", 4) == "abc…" &&
               core::hover_tip_clipped("あいう", 2) == "あい…" &&
               core::hover_tip_clipped("abc", 0) == "…",
           "the clipped text keeps whole characters, trims the end and adds an ellipsis");
    expect(
        core::hover_tip_text_limit(2000, 96) == 460 && core::hover_tip_text_limit(300, 96) == 264 &&
            core::hover_tip_text_limit(10, 96) == 0 && core::hover_tip_text_limit(2000, 192) == 920,
        "the text limit is 480 DIP or the area less the edges, less the padding");
    const core::LayoutRect area{100, 50, 900, 600};
    const core::TabTipMeasure text{200, 40, 96};
    // 札は 220 × 52。上は キャレットの上端 − 4 − 52。
    expect(core::hover_tip_rect(core::LayoutRect{300, 200, 302, 220}, area, text) ==
                   core::LayoutRect{300, 144, 520, 196} &&
               core::hover_tip_rect(core::LayoutRect{300, 60, 302, 80}, area, text) ==
                   core::LayoutRect{300, 84, 520, 136} &&
               core::hover_tip_rect(core::LayoutRect{800, 200, 802, 220}, area, text) ==
                   core::LayoutRect{672, 144, 892, 196} &&
               core::hover_tip_rect(core::LayoutRect{20, 200, 22, 220},
                                    core::LayoutRect{100, 50, 250, 600},
                                    text) == core::LayoutRect{100, 144, 320, 196},
           "the tip sits above the caret, below it when there is no room, inside the area");
    expect(core::hover_tip_rect(core::LayoutRect{300, 60, 302, 80},
                                core::LayoutRect{100, 50, 900, 120},
                                text) == core::LayoutRect{300, 68, 520, 120} &&
               core::hover_tip_rect(core::LayoutRect{300, 60, 302, 80},
                                    core::LayoutRect{100, 50, 900, 90},
                                    text) == core::LayoutRect{300, 50, 520, 102},
           "a tip that fits neither above nor below is moved up to the bottom of the area");
}

// ---------------------------------------------- #1843 のレビューの直し（R1〜R3・R5・R6）

// 窓の 1 打鍵と同じ意図（apply）で Vim の文字の鍵を流す。
void keyed(Editing &editing, std::u32string_view keys)
{
    for (const char32_t code : keys)
    {
        static_cast<void>(editing.controller().apply_frame(
            app::VimKeyPress{core::VimKey{core::VimCharacter{code}}}));
    }
}

// `gd` の後に keys を打ってから、定義の要求に two.cpp の場所で答えた後のアクティブな経路。
[[nodiscard]] std::string defined_after(std::u32string_view keys)
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    keyed(editing, U"gd");
    const std::int64_t asked = last_request(editing, "textDocument/definition");
    keyed(editing, keys);
    const auto frame = answered_frame(editing, asked, two_location);
    return message(frame).empty() ? active_path(editing) : "<message>";
}

// R1: 答えが届いたときに Vim が何かを待っていれば跳ばない（知らせない）。
void verify_definition_waits_for_nothing()
{
    expect(defined_after(U"") == second_path, "an idle NORMAL lands on the answer");
    expect(defined_after(U"d") == source_path, "a pending operator drops the definition answer");
    expect(defined_after(U"v") == source_path, "VISUAL drops the definition answer");
    expect(defined_after(U"i") == source_path, "INSERT drops the definition answer");
    expect(defined_after(U"\"a") == source_path, "a chosen register drops the definition answer");
    expect(defined_after(U"3") == source_path, "a count drops the definition answer");
    expect(defined_after(U"g") == source_path, "a pending g drops the definition answer");
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    keyed(editing, U"gdd");
    static_cast<void>(
        answered_frame(editing, last_request(editing, "textDocument/definition"), two_location));
    keyed(editing, U"d");
    expect(active_path(editing) == source_path &&
               whole_vim_body(editing.controller()) != "int main() {}\n",
           "gd, d, a late answer and d delete the line in the document that was asked about");

    // `d` で始まる map を持つと、`gd` の後の `d` は engine に届かず積まれる（再確認 P1）。
    Editing mapped(lsp_settings(true));
    capable_vim(mapped);
    static_cast<void>(run_ex(mapped.controller(), "nnoremap dq x"));
    keyed(mapped, U"gd");
    const std::int64_t waited = last_request(mapped, "textDocument/definition");
    keyed(mapped, U"d");
    static_cast<void>(answered_frame(mapped, waited, two_location));
    expect(!mapped.controller().vim_state().pending.has_value() &&
               active_path(mapped) == source_path,
           "a key held for a map drops the definition answer");

    Editing plain(lsp_settings(true));
    capable(plain);
    static_cast<void>(
        plain.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition}));
    const std::int64_t asked = last_request(plain, "textDocument/definition");
    static_cast<void>(plain.controller().apply_frame(app::OpenContextMenu{10, 40, std::nullopt}));
    static_cast<void>(answered_frame(plain, asked, two_location));
    expect(active_path(plain) == source_path,
           "an open context menu drops the definition answer in the ordinary mode");
}

// R2 / R3 / R6: 入力行が開いている間・窓が非アクティブの間に届いた hover は札にしない。空白だけの
// 答えは「情報がありません」。
void verify_hover_answer_needs_a_free_window()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    keyed(editing, U"K");
    const std::int64_t lined = last_request(editing, "textDocument/hover");
    keyed(editing, U":");
    const auto with_line = answered_frame(editing, lined, R"({"contents":"doc"})");
    expect(!with_line.hover.has_value() &&
               !editing.controller().language_answers().hover.has_value(),
           "a hover answer that arrives while a command line is open is dropped");
    static_cast<void>(editing.controller().apply_frame(app::CancelCommand{}));
    keyed(editing, U"K");
    const std::int64_t away = last_request(editing, "textDocument/hover");
    static_cast<void>(editing.controller().apply_frame(app::WindowDeactivated{}));
    const auto inactive = answered_frame(editing, away, R"({"contents":"doc"})");
    expect(!inactive.hover.has_value() && message(inactive).empty() &&
               !editing.controller().language_answers().hover.has_value(),
           "a hover answer that arrives in an inactive window is dropped without a notice");
    keyed(editing, U"K");
    const auto back = answered_frame(editing, last_request(editing, "textDocument/hover"),
                                     R"({"contents":"doc"})");
    expect(back.hover == std::optional<std::string>{"doc"},
           "a key in the window makes it active again and the next hover shows");
    keyed(editing, U"K");
    const auto blank = answered_frame(editing, last_request(editing, "textDocument/hover"),
                                      R"({"contents":"  \n \t "})");
    expect(!blank.hover.has_value() && message(blank) == "情報がありません",
           "a hover of white space only shows no tip and tells there is no information");
}

// R5: 録画と再生・`.`・`map` の右辺の `gd` `K`。再生は失敗の印で後ろの鍵を捨てる（表に無い鍵と
// 同じ）が、問いは出る。`.` の記録には載らない。
void verify_queries_in_replays()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    const auto &server = editing.language_server();
    keyed(editing, U"qaKxq");
    expect(server.requests("textDocument/hover").size() == 1 &&
               whole_vim_body(editing.controller()) == "nt main() {}\n",
           "K is recorded like any key and the typed x after it runs");
    keyed(editing, U"@a");
    expect(server.requests("textDocument/hover").size() == 2 &&
               whole_vim_body(editing.controller()) == "nt main() {}\n",
           "a replayed K asks again and ends the replay like an unbound key");
    keyed(editing, U"xgd.");
    expect(server.requests("textDocument/definition").size() == 1 &&
               whole_vim_body(editing.controller()) == " main() {}\n",
           "gd is not the last change: the dot repeats the x before it");
    static_cast<void>(run_ex(editing.controller(), "nnoremap Q gd"));
    static_cast<void>(run_ex(editing.controller(), "nnoremap Z Kx"));
    keyed(editing, U"Q");
    expect(server.requests("textDocument/definition").size() == 2,
           "a map whose right side is gd asks for the definition");
    keyed(editing, U"Z");
    expect(server.requests("textDocument/hover").size() == 3 &&
               whole_vim_body(editing.controller()) == " main() {}\n",
           "K in a map asks and drops the rest of the map like an unbound key");

    Editing off;
    open_vim_document(off, "abc def\n");
    keyed(off, U"qaKxq@a");
    expect(whole_vim_body(off.controller()) == "bc def\n" && off.language_server().touches() == 0,
           "without a server the recorded K fails in the replay as it did before");
}

// R5: 2 枠に分けていても、操作中の枠のキャレットで問い、その枠が着地する。
void verify_definition_in_split()
{
    Editing editing(lsp_settings(true));
    capable_vim(editing);
    applied(editing.controller(), app::SplitPane{core::SplitLayout::vertical});
    const auto frame = defined(editing, main_location);
    expect(landed_at(frame, 1, 5) && message(frame).empty(),
           "gd lands in the active pane of a split window");
    const auto tip = hovered(editing, R"({"contents":"doc"})");
    expect(tip.hover == std::optional<std::string>{"doc"}, "K shows the tip in a split window");
}

// R5: まだ読んでいないタブの文書への着地は、読んでから着地する（reach_tab の道）。
void verify_definition_in_unread_tab()
{
    const auto listed = [](std::string_view path, std::size_t rank)
    {
        return app::SessionTab{path_of(path),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    Editing editing(lsp_settings(true),
                    SessionReading{std::optional<app::Session>{
                        app::Session{{listed(source_path, 0), listed(second_path, 1)}, 0}}},
                    [](ScriptedFiles &files)
                    {
                        files.hold_at(std::string(source_path),
                                      Bytes{std::string("int main() {}\n")});
                        files.hold_at(std::string(second_path), Bytes{std::string("abc\n")});
                    });
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(editing, app::ServerMessage{std::string{capable_answer}}));
    const std::size_t reads = editing.files().reads();
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition}));
    const auto frame =
        answered_frame(editing, last_request(editing, "textDocument/definition"), two_location);
    expect(active_path(editing) == second_path && landed_at(frame, 1, 3) &&
               editing.files().reads() > reads,
           "a definition in a tab not read yet reads the tab and lands in it");
}

// 補完の面と hover の札は同時に出ない。補完の面が開いている間は定義の答えで跳ばない。
void verify_queries_yield_to_completion()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::hover}));
    const auto tip = answered_frame(editing, last_request(editing, "textDocument/hover"),
                                    R"({"contents":"doc"})");
    const auto opened = editing.controller().apply_frame(app::OpenCompletion{});
    expect(tip.hover.has_value() && editing.controller().completion_open() &&
               !opened.hover.has_value() &&
               !editing.controller().language_answers().hover.has_value(),
           "opening the completion face drops the hover tip");
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::hover}));
    const std::int64_t hover = last_request(editing, "textDocument/hover");
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition}));
    const std::int64_t definition = last_request(editing, "textDocument/definition");
    static_cast<void>(editing.controller().apply_frame(app::OpenCompletion{}));
    const auto late = answered_frame(editing, hover, R"({"contents":"doc"})");
    static_cast<void>(answered_frame(editing, definition, two_location));
    expect(editing.controller().completion_open() && !late.hover.has_value() &&
               !editing.controller().language_answers().hover.has_value() &&
               active_path(editing) == source_path,
           "answers that arrive while the completion face is open show no tip and do not jump");
}

void verify_language_query_reviews()
{
    verify_queries_yield_to_completion();
    verify_definition_waits_for_nothing();
    verify_hover_answer_needs_a_free_window();
    verify_queries_in_replays();
    verify_definition_in_split();
    verify_definition_in_unread_tab();
}

void verify_language_queries()
{
    verify_query_tables();
    verify_vim_keys_ask();
    verify_vim_keys_fail_without_server();
    verify_definition_in_document();
    verify_definition_in_other_file();
    verify_definition_notices();
    verify_definition_context();
    verify_hover_tip_shown();
    verify_hover_tip_edges();
    verify_query_operations();
    verify_hover_tip_core();
    verify_language_query_reviews();
}
// 2026-11-20 の追補（#1897）: `textDocumentSync` の種類に従う。none（数の 0・欄が無い）は
// didChange を送らず、full は range の無い全文を 1 つ、object の `change` も読む。
void verify_document_sync_kinds()
{
    const auto changes_after_typing = [](std::string_view capabilities)
    {
        Editing editing(lsp_settings(true));
        started_with(editing, capabilities);
        applied(editing.controller(), app::InsertText{"x"});
        applied(editing.controller(), app::InsertText{"y"});
        return editing.language_server().notifications("textDocument/didChange");
    };
    expect(changes_after_typing("{}").empty() &&
               changes_after_typing(R"({"textDocumentSync":0})").empty() &&
               changes_after_typing(R"({"textDocumentSync":{"openClose":true}})").empty(),
           "a server without textDocumentSync, or with None, gets no didChange");
    const auto full = changes_after_typing(R"({"textDocumentSync":1})");
    expect(full.size() == 2 &&
               contains(full.back(), R"("contentChanges":[{"text":"xyint main() {}\n"}])") &&
               version_in(full.back()) == std::optional<std::uint64_t>{2},
           "a Full server gets the whole text as one change without a range");
    const auto object = changes_after_typing(R"({"textDocumentSync":{"change":1,"save":true}})");
    expect(object.size() == 2 && !contains(object.back(), R"("range")"),
           "the change kind of TextDocumentSyncOptions is read too");
    const auto incremental = changes_after_typing(R"({"textDocumentSync":{"change":2}})");
    expect(incremental.size() == 2 && contains(incremental.back(), R"("range")"),
           "an Incremental server keeps getting ranges");
}

// 2026-11-20 の追補の限界（#1907 のレビュー L3）: none のサーバーの診断は didOpen
// の版の本文で写し、 編集でずらさずに今の本文へ置く。本文の外を指す診断でも落ちない。
void verify_unsynced_diagnostics()
{
    Editing editing(lsp_settings(true));
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
    started_with(editing, R"({"textDocumentSync":0})");
    applied(editing.controller(), app::InsertText{"xy"});
    auto &server = editing.language_server();
    server.publish(source_uri, 0, main_error);
    const auto placed = editing.controller().apply_frame(app::WorkCompleted{});
    expect(server.notifications("textDocument/didChange").empty() &&
               underlined(placed.lines.at(0), 5, 9, core::DiagnosticSeverity::error),
           "an unsynced server's diagnostics are placed by the opened text without shifting");
    server.publish(
        source_uri, std::nullopt,
        R"([{"range":{"start":{"line":7,"character":40},"end":{"line":9,"character":2}},"severity":1,"message":"far"}])");
    const auto far = editing.controller().apply_frame(app::WorkCompleted{});
    expect(far.lines.size() == placed.lines.size(),
           "a diagnostic past the current text is placed without breaking the frame");
}

// 2026-11-20 の追補（#1897）: `save` を求めるサーバーには保存の後に `didSave`（`includeText` なら
// 本文つき）。求めないサーバー・別のパスへの保存では送らない。積んだ変更を先に流す。
void verify_did_save()
{
    const auto saved_with = [](std::string_view capabilities, std::string_view path)
    {
        Editing editing(lsp_settings(true));
        started_with(editing, capabilities);
        applied(editing.controller(), app::InsertText{"x"});
        static_cast<void>(editing.controller().apply_frame(
            app::SaveDocument{path_of(path), core::TextEncoding::utf8}));
        return editing.language_server().sent();
    };
    const auto plain = saved_with(R"({"textDocumentSync":{"change":2,"save":true}})", source_path);
    expect(
        contains(
            plain.back(),
            R"("method":"textDocument/didSave","params":{"textDocument":{"uri":"file:///C:/work/main.cpp"}})") &&
            contains(plain.at(plain.size() - 2), R"("method":"textDocument/didChange")"),
        "saving sends didSave after the pending changes");
    const auto text =
        saved_with(R"({"textDocumentSync":{"change":2,"save":{"includeText":true}}})", source_path);
    expect(contains(text.back(), R"("method":"textDocument/didSave")") &&
               contains(text.back(), R"("text":"xint main() {}\n")"),
           "includeText adds the saved text");
    const auto quiet = saved_with(R"({"textDocumentSync":2})", source_path);
    const auto other =
        saved_with(R"({"textDocumentSync":{"change":2,"save":true}})", R"(C:\work\renamed.cpp)");
    const auto said = [](const std::vector<std::string> &sent)
    {
        return std::ranges::any_of(
            sent, [](const std::string &body)
            { return contains(body, R"("method":"textDocument/didSave")"); });
    };
    expect(!said(quiet) && !said(other),
           "no didSave without the save ability or when saving under another name");
}

// 2026-11-20 の追補（#1897）: 診断の URI は定義の着地と同じ規則（パスへ写して same_file）で照合し、
// ドライブ文字の大小と百分率の符号化の違いで捨てない。
void verify_diagnostics_uri_forms()
{
    for (const std::string_view uri : {std::string_view{"file:///c:/work/main.cpp"},
                                       std::string_view{"file:///C%3A/work/main.cpp"},
                                       std::string_view{"file:///c%3a/work/%6Dain.cpp"}})
    {
        Editing editing(lsp_settings(true));
        started_with_lines(editing);
        editing.language_server().publish(uri, 0, main_error);
        const auto frame = editing.controller().apply_frame(app::WorkCompleted{});
        expect(underlined(frame.lines.at(0), 5, 9, core::DiagnosticSeverity::error),
               "diagnostics for another spelling of the same file URI are placed");
    }
}

// 2026-11-20 の追補（#1897）: 起こせない理由を分けて 1 行で知らせる。
void verify_start_failure_notices()
{
    const auto told = [](app::ServerFailure failure)
    {
        Editing editing(lsp_settings(true));
        hold_documents(editing);
        editing.language_server().refuse(failure);
        return message(editing.controller().apply_frame(app::OpenDocument{path_of(source_path)}));
    };
    expect(
        told(app::ServerFailure::not_found) == "言語サーバーの実行ファイルがありません: cpp" &&
            told(app::ServerFailure::not_absolute) ==
                "言語サーバーの命令は絶対パスで書きます: cpp" &&
            told(app::ServerFailure::script_refused) ==
                "言語サーバーに .cmd / .bat は使えません（node.exe とスクリプトで書きます）: cpp" &&
            told(app::ServerFailure::pipe_failed) == "言語サーバーを起こせませんでした: cpp",
        "missing, relative, script and other start failures are told apart");
}

// 2026-11-20 の追補（#1897）: 表に無い名前の命令の行（後の版が足す言語）は使わない。
void verify_unknown_command_rows_unused()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"java", R"(C:\jdtls\jdtls.exe)"}};
    Editing editing(SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}});
    hold_documents(editing);
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    expect(editing.language_server().launches().empty(),
           "a command row for a language this version does not know starts nothing");
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
    verify_question_fold_versions();
    verify_did_close();
    verify_saved_as();
    verify_diagnostics_placed();
    verify_diagnostics_follow_edits();
    verify_parked_diagnostics();
    verify_diagnostic_frame();
    verify_show_message_notices();
    verify_recording_hides_message();
    verify_truncated_count();
    verify_line_end_diagnostic();
    verify_abilities();
    verify_requests_need_an_open_document();
    verify_request_params();
    verify_answer_context();
    verify_cancel();
    verify_completion_answer();
    verify_request_expiry();
    verify_language_queries();
    verify_abilities_one_by_one();
    verify_requests_need_a_sent_document();
    verify_answers_after_restart();
    verify_document_sync_kinds();
    verify_unsynced_diagnostics();
    verify_did_save();
    verify_diagnostics_uri_forms();
    verify_start_failure_notices();
    verify_unknown_command_rows_unused();
}

void verify_language_server_scope()
{
    verify_language_server_contracts();
}
} // namespace nenenib::tests
