// 契約 --language-server の記録と進み具合の部分（ADR 0160 の 2026-11-22 の追補・LSP 段 S8・
// #1942）。core の server_log_line・lsp_logged_message・lsp_progress_of・lsp_progress_text・
// status_server_item と、application の LanguageServerLog の輪と、controller の `:LspLog`・F1 の
// 操作・`window.workDoneProgress`・`window/workDoneProgress/create`・`$/progress`
// のステータスバーの 1 行を、替え玉 ScriptedLanguageServer
// で確かめる。本物のサーバーも窓も使わない。
#include "Appearance.hpp"
#include "DiagnosticCounts.hpp"
#include "DiagnosticsCount.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "ExLspRequest.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "JsonParse.hpp"
#include "Language.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerLog.hpp"
#include "LanguageServerLogKind.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageServerTick.hpp"
#include "LayoutRect.hpp"
#include "LspIncoming.hpp"
#include "LspMessageType.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspProgress.hpp"
#include "LspProgressKind.hpp"
#include "LspShowMessage.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "OpenLanguageServerLog.hpp"
#include "OperationBindings.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ServerErrorOutput.hpp"
#include "ServerEvent.hpp"
#include "ServerExited.hpp"
#include "ServerLogLine.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "StatusBarLayout.hpp"
#include "StatusServerItem.hpp"
#include "TestSupport.hpp"
#include "UnsavedTab.hpp"
#include "Utf8.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe")";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] SettingsReading lsp_settings(bool on)
{
    auto settings = core::default_editor_settings();
    settings.lsp = on;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

app::EditorFrame delivered(Editing &editing, app::ServerEvent event)
{
    editing.language_server().serve(std::move(event));
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

[[nodiscard]] std::string status_of(const app::EditorFrame &frame)
{
    return frame.language_server_status.has_value()
               ? std::string(frame.language_server_status.value().text())
               : "<none>";
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

// 開いて起こす（`initialize` の答えはまだ）。
void opened(Editing &editing)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string("int main() {}\n")});
    applied(editing.controller(), app::OpenDocument{path_of(source_path)});
    // root を探した答えの合図で起きる（ADR 0161 の 2026-11-22 の追補・段 S3）。
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// `initialize` に答えて running まで進める。
void running(Editing &editing)
{
    opened(editing);
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2}}})"}));
}

[[nodiscard]] core::LspNotification notification_of(std::string_view body)
{
    const auto value = core::parse_json(body);
    const auto incoming = core::lsp_incoming(value.value());
    return std::get<core::LspNotification>(incoming.value());
}

[[nodiscard]] std::string progress_body(std::string_view token, std::string_view value)
{
    return R"({"jsonrpc":"2.0","method":"$/progress","params":{"token":)" + std::string(token) +
           R"(,"value":)" + std::string(value) + "}}";
}

// 1 行の直し の端（#2013）: 列を始めないバイト・継続の欠けた列・4 バイトの字・終わりの無い CSI と
// OSC・ESC \ で閉じる OSC・C1 でない 2 バイトと双方向の制御でない 3 バイト。
void verify_server_log_line_edges()
{
    const std::array<std::pair<std::string_view, std::string_view>, 11> lines{{
        {"\xF5x", "\xEF\xBF\xBDx"},
        {"\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"},
        {"\xE3\x41\x41", "\xEF\xBF\xBD\x41\x41"},
        {"a\x1b[", "a"},
        {"a\x1b[12", "a"},
        {"a\x1b[1\x01", "a\xE2\x90\x81"},
        {"\x1b]0;t\x1b\\ok", "ok"},
        {"\x1b]0;title", ""},
        {"\x1b]0;t\x1b", ""},
        {"\xC2\xA0\xC3\xA9", "\xC2\xA0\xC3\xA9"},
        {"\xE2\x81\xA5", "\xE2\x81\xA5"},
    }};
    for (const auto &[raw, shown] : lines)
    {
        expect(core::server_log_line(raw) == shown, "each raw log line has its repaired form");
    }
}

// 1 行の直し: 不正な列・制御文字・長い行（追補）。
void verify_server_log_line()
{
    expect(core::server_log_line("plain\ttab") == "plain\ttab",
           "a valid line keeps its text and its tabs");
    expect(core::server_log_line("a\xff"
                                 "b\x01"
                                 "c\x7f") == "a\xEF\xBF\xBD"
                                             "b\xE2\x90\x81"
                                             "c\xE2\x90\xA1",
           "a broken byte becomes U+FFFD and C0 and DEL become control pictures");
    expect(core::server_log_line("\x1b[31mred\x1b[0m \x1b]0;title\x07"
                                 "ok \x1b"
                                 "c!") == "red ok !",
           "ANSI escape sequences (CSI, OSC and ESC with one character) are dropped");
    expect(core::server_log_line("x\x1b") == "x\xE2\x90\x9B",
           "a lone ESC is shown as its control picture");
    expect(core::server_log_line("a\xC2\x85"
                                 "b\xE2\x80\xAE"
                                 "c\xE2\x81\xA6"
                                 "d\xE2\x80\x8B") == "a<U+0085>b<U+202E>c<U+2066>d\xE2\x80\x8B",
           "C1 and the bidirectional controls are shown as code points and others are kept");
    expect(core::server_log_line("\xE3\x81\x82\xE3\x81") == "\xE3\x81\x82\xEF\xBF\xBD\xEF\xBF\xBD",
           "a valid sequence stays and a cut sequence is replaced byte by byte");
    const std::string long_line = core::server_log_line(std::string(5000, 'x'));
    expect(long_line.size() <= core::server_log_line_bytes && long_line.ends_with("\xE2\x80\xA6") &&
               core::validate_utf8(long_line).has_value(),
           "a long line is cut within the limit and marked with an ellipsis");
    std::string wide;
    for (std::size_t index = 0; index < 1000; ++index)
    {
        wide += "\xE3\x81\x82";
    }
    const std::string cut = core::server_log_line(wide);
    expect(cut.size() <= core::server_log_line_bytes && core::validate_utf8(cut).has_value(),
           "a long line of three byte characters is cut on a code point boundary");
    verify_server_log_line_edges();
}

// 席ごとの輪（追補）。標準エラーの残り・CR LF・閉じ・上限・行の頭の印。
void verify_log_ring()
{
    app::LanguageServerLog log;
    expect(log.empty() && log.text().empty(), "a new log is empty");
    const auto first = app::LanguageServerSlot::first;
    const auto second = app::LanguageServerSlot::second;
    log.add_output(first, core::Language::cpp, "I[1] one\r\nI[2] tw");
    expect(log.text() == "[cpp][stderr] I[1] one\r\n",
           "only finished lines are kept and the CR of CR LF is dropped");
    log.add_output(first, core::Language::cpp, "o\n");
    log.add_message(second, core::Language::python, app::LanguageServerLogKind::info,
                    "jedi\nready\n");
    log.add_output(first, core::Language::cpp, "tail without newline");
    log.end_output(first, core::Language::cpp);
    log.end_output(first, core::Language::cpp);
    expect(log.text() == "[cpp][stderr] I[1] one\r\n[cpp][stderr] I[2] two\r\n"
                         "[cpp][stderr] tail without newline\r\n"
                         "[python][info] jedi\r\n[python][info] ready\r\n",
           "the slots are listed in order and end_output closes the rest once");
    log.add_output(first, core::Language::cpp, std::string(10000, 'y') + "\nafter\n");
    const std::string text = log.text();
    expect(contains(text, "\xE2\x80\xA6\r\n[cpp][stderr] after\r\n"),
           "a stderr line longer than the limit is cut and the next line starts fresh");
    app::LanguageServerLog full;
    for (std::size_t index = 0; index < app::language_server_log_lines + 10; ++index)
    {
        full.add_message(first, core::Language::cpp, app::LanguageServerLogKind::log,
                         "line " + std::to_string(index));
    }
    const std::string kept = full.text();
    expect(!contains(kept, "] line 9\r\n") && contains(kept, "] line 10\r\n") &&
               contains(kept, "] line 521\r\n"),
           "the oldest lines are dropped beyond 512 lines");
    app::LanguageServerLog heavy;
    for (std::size_t index = 0; index < 400; ++index)
    {
        heavy.add_message(first, core::Language::cpp, app::LanguageServerLogKind::log,
                          std::string(1000, 'z'));
    }
    expect(heavy.text().size() <= app::language_server_log_bytes + 400 * 2,
           "a slot keeps at most 256 KiB of lines");
}

// 知らせの通知 → 種類と文（追補）。
void verify_logged_messages()
{
    const auto logged = [](std::string_view body)
    { return core::lsp_logged_message(notification_of(body)); };
    const auto log = logged(
        R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"type":4,"message":"hi"}})");
    expect(log.has_value() && log.value().first == core::LspMessageType::log &&
               log.value().second == "hi",
           "logMessage type 4 is a log line");
    const auto shown = logged(
        R"({"jsonrpc":"2.0","method":"window/showMessage","params":{"type":3,"message":"x"}})");
    expect(shown.has_value() && shown.value().first == core::LspMessageType::info,
           "showMessage info is logged too");
    const auto debug = logged(
        R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"type":5,"message":"d"}})");
    expect(debug.has_value() && debug.value().first == core::LspMessageType::log,
           "an unknown type is a log line");
    expect(!logged(R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"message":"m"}})")
                   .has_value() &&
               !logged(R"({"jsonrpc":"2.0","method":"other","params":{"type":1,"message":"m"}})")
                    .has_value(),
           "a message without a type and other methods are not logged");
}

// `$/progress` の読みと 1 行（追補）。
void verify_progress_values()
{
    const auto begin = core::lsp_progress_of(notification_of(progress_body(
        R"("idx")", R"({"kind":"begin","title":"indexing","message":"1/9","percentage":150})")));
    expect(begin.has_value() && begin.value().kind == core::LspProgressKind::begin &&
               begin.value().token == core::LspId{std::string("idx")} &&
               begin.value().percentage == std::optional<std::uint64_t>{100},
           "begin carries the token, the title and a percentage clamped to 100");
    const auto numbered = core::lsp_progress_of(
        notification_of(progress_body("7", R"({"kind":"end","message":"done"})")));
    expect(numbered.has_value() && numbered.value().token == core::LspId{std::int64_t{7}} &&
               numbered.value().kind == core::LspProgressKind::end,
           "an integer token and end are read");
    expect(!core::lsp_progress_of(notification_of(progress_body("1", R"({"kind":"other"})")))
                   .has_value() &&
               !core::lsp_progress_of(notification_of(progress_body("1", R"([1,2])"))).has_value(),
           "an unknown kind and a partial result are not progress");
    const auto shown =
        begin.has_value() ? core::lsp_progress_text("cpp", begin.value()) : std::nullopt;
    expect(shown.has_value() && shown.value().text() == "cpp: indexing 1/9 100%",
           "the status line names the language, the title, the message and the percentage");
    core::LspProgressValue bare{core::LspId{std::int64_t{1}}, core::LspProgressKind::report,
                                std::nullopt, std::nullopt, std::nullopt};
    expect(!core::lsp_progress_text("cpp", bare).has_value(),
           "progress without any text shows nothing");
}

// ステータスバーの欄の形（追補・#1950 のレビュー R2 / R3）。言語サーバーの受けている文書の欄は
// 件数より広い一定の幅で、件数の後ろに進み具合を並べる。
void verify_status_item()
{
    const core::DiagnosticCounts none{0, 0, core::DiagnosticsCount::complete};
    const core::DiagnosticCounts some{2, 1, core::DiagnosticsCount::complete};
    expect(core::status_server_item(none, false) == core::StatusServerItem::none &&
               core::status_server_item(some, false) == core::StatusServerItem::counts &&
               core::status_server_item(none, true) == core::StatusServerItem::server &&
               core::status_server_item(some, true) == core::StatusServerItem::server,
           "a served document has the server field whether or not it has counts");
    const auto counted = core::status_bar_layout(1280, 800, 96, core::StatusServerItem::counts);
    const auto served = core::status_bar_layout(1280, 800, 96, core::StatusServerItem::server);
    const auto empty = core::status_bar_layout(1280, 800, 96, core::StatusServerItem::none);
    expect(core::width_of(counted.diagnostics) == 96 && core::width_of(served.diagnostics) == 168 &&
               core::width_of(empty.diagnostics) == 0 &&
               served.diagnostics.right == counted.diagnostics.right,
           "the server field is the same right edge field, wider than the counts");
    expect(counted.items == core::status_bar_layout(1280, 800, 96, some).items,
           "the counts overload lays out like the counts item");
    const auto narrow = core::status_bar_layout(360, 400, 96, core::StatusServerItem::server);
    expect(core::width_of(narrow.diagnostics) == 0,
           "a narrow window folds the server field like the counts");
    bool fallback = false;
    bool kept_counts = true;
    for (std::int32_t width = 300; width <= 1000; width += 4)
    {
        const auto wide = core::width_of(
            core::status_bar_layout(width, 800, 96, core::StatusServerItem::server).diagnostics);
        const auto counts = core::width_of(
            core::status_bar_layout(width, 800, 96, core::StatusServerItem::counts).diagnostics);
        fallback = fallback || wide == 96;
        kept_counts = kept_counts && (wide == 168 || wide == counts);
    }
    expect(fallback && kept_counts,
           "a window too narrow for the server field gives it the counts width before folding");
    const auto status = core::DisplayText::parse("cpp: indexing 33%").value();
    const auto text = [](const std::optional<core::DisplayText> &shown)
    { return shown.has_value() ? std::string(shown.value().text()) : "<none>"; };
    expect(text(core::status_server_text(some, status)) == "E2 W1 \xC2\xB7 cpp: indexing 33%" &&
               text(core::status_server_text(none, status)) == "cpp: indexing 33%" &&
               text(core::status_server_text(some, std::nullopt)) == "E2 W1" &&
               text(core::status_server_text(none, std::nullopt)) == "<none>",
           "the counts come first and stay while progress shows, progress alone without counts");
}

// `:LspLog` の Ex（追補）。
void verify_ex_lsp_log()
{
    const auto settings = core::default_editor_settings();
    const auto parsed = core::evaluate_ex("LspLog", settings, core::Appearance::dark);
    expect(parsed.has_value() && parsed.value().lsp == std::optional{core::ExLspRequest::log} &&
               !parsed.value().settings.has_value(),
           ":LspLog is the log request and changes no setting");
    expect(!core::evaluate_ex("LspLog x", settings, core::Appearance::dark).has_value() &&
               !core::evaluate_ex("2LspLog", settings, core::Appearance::dark).has_value() &&
               !core::evaluate_ex("lsplog", settings, core::Appearance::dark).has_value(),
           "an argument, a range and the lower case name are refused");
    expect(core::operation_available(core::EditorOperation::open_language_server_log,
                                     core::EditMode::ordinary) &&
               core::operation_available(core::EditorOperation::open_language_server_log,
                                         core::EditMode::vim),
           "the log operation is in the F1 list of both modes");
}

// 標準エラーと知らせを記録して無題のタブに開く（追補）。
void verify_open_log()
{
    Editing editing(lsp_settings(true));
    running(editing);
    static_cast<void>(editing.controller().apply_frame(app::VisibleLines{40}));
    static_cast<void>(delivered(editing, app::ServerErrorOutput{"I[0] start\nE[1] bro"}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"type":4,"message":"idle"}})"}));
    const auto info = delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","method":"window/showMessage","params":{"type":3,"message":"hello"}})"});
    expect(message_of(info).empty(), "an info showMessage is logged but not told");
    editing.language_server().serve(app::ServerErrorOutput{"ken\n"});
    const std::size_t tabs = editing.controller().frame().tabs.size();
    const std::string text = applied(editing.controller(), app::OpenLanguageServerLog{});
    const auto frame = editing.controller().frame();
    expect(text == "[cpp][stderr] I[0] start|[cpp][log] idle|[cpp][info] hello|"
                   "[cpp][stderr] E[1] broken|",
           "the log opens with the lines in arrival order, the pending stderr collected "
           "first");
    expect(frame.tabs.size() == tabs + 1 && frame.active_tab == tabs &&
               frame.document.save_state == core::SaveState::saved &&
               app::unsaved_tab_count(frame.tabs) == 0,
           "the copy is a new untitled tab that is not modified, so closing does not ask");
    const auto again = run_ex(editing.controller(), "LspLog");
    expect(again.tabs.size() == tabs + 2 && again.document.save_state == core::SaveState::saved,
           ":LspLog opens another copy through the same path");
    static_cast<void>(delivered(editing, app::ServerErrorOutput{"last words"}));
    static_cast<void>(delivered(editing, app::ServerExited{3}));
    const std::string after = applied(editing.controller(), app::OpenLanguageServerLog{});
    expect(contains(after, "|[cpp][stderr] last words|"),
           "the unfinished last stderr line is closed when the server ends");
    // 何も書いていない無題からは、ファイルを開く道と同じく置き換える（#1950 のレビュー L4）。
    static_cast<void>(applied(editing.controller(), app::NewTab{}));
    const std::size_t blank = editing.controller().frame().tabs.size();
    const std::string replaced = applied(editing.controller(), app::OpenLanguageServerLog{});
    expect(editing.controller().frame().tabs.size() == blank && contains(replaced, "last words"),
           "from a blank untitled tab the log replaces it like opening a file");
}

// 記録を開いて写しの文（行は | で繋ぐ）。
[[nodiscard]] std::string log_of(Editing &editing)
{
    static_cast<void>(editing.controller().apply_frame(app::VisibleLines{40}));
    return applied(editing.controller(), app::OpenLanguageServerLog{});
}

void ticked(Editing &editing, std::size_t times)
{
    for (std::size_t tick = 0; tick < times; ++tick)
    {
        static_cast<void>(editing.controller().apply_frame(app::LanguageServerTick{}));
    }
}

// 止める道は、まだ集めていない標準エラーと logMessage を記録に入れてから止める（#1950 の
// レビュー M1）。
void verify_stop_keeps_unread_log()
{
    constexpr std::string_view log_message =
        R"({"jsonrpc":"2.0","method":"window/logMessage","params":{"type":1,"message":"no compile_commands.json"}})";
    {
        // `initialize` に答えず標準エラーにだけ理由を書くサーバーを tick が止める。
        Editing editing(lsp_settings(true));
        opened(editing);
        static_cast<void>(delivered(editing, app::ServerStarted{}));
        editing.language_server().serve(app::ServerErrorOutput{"cannot read config\nhalf"});
        editing.language_server().serve(app::ServerMessage{std::string(log_message)});
        ticked(editing, app::language_server_stale_ticks);
        const std::string text = log_of(editing);
        expect(
            text == "[cpp][stderr] cannot read config|[cpp][error] no compile_commands.json|"
                    "[cpp][stderr] half|",
            "the silent server stopped by the tick leaves its stderr and log message in the log");
    }
    {
        // `:set nolsp` で `shutdown` を送り、答えないので tick が止める。
        Editing editing(lsp_settings(true));
        running(editing);
        static_cast<void>(run_ex(editing.controller(), "set nolsp"));
        editing.language_server().serve(app::ServerErrorOutput{"shutting down\n"});
        ticked(editing, app::language_server_stale_ticks);
        expect(contains(log_of(editing), "[cpp][stderr] shutting down|"),
               "a server that does not answer shutdown leaves its stderr in the log");
    }
    {
        // `shutdown` の答えと同じ集めの後ろの塊。
        Editing editing(lsp_settings(true));
        running(editing);
        static_cast<void>(run_ex(editing.controller(), "set nolsp"));
        auto &server = editing.language_server();
        server.answer(server.requests("shutdown").back(), "null");
        server.serve(app::ServerErrorOutput{"after the answer"});
        server.serve(app::ServerMessage{std::string(log_message)});
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
        const std::string text = log_of(editing);
        expect(contains(text, "[cpp][stderr] after the answer|") &&
                   contains(text, "[cpp][error] no compile_commands.json|"),
               "what came after the shutdown answer in the same collect is logged, not read");
    }
    {
        // 起こしている間に lsp をオフにする（`initialize` の前は何も送れないのですぐ止める）。
        Editing editing(lsp_settings(true));
        opened(editing);
        editing.language_server().serve(app::ServerErrorOutput{"early\n"});
        static_cast<void>(run_ex(editing.controller(), "set nolsp"));
        expect(contains(log_of(editing), "[cpp][stderr] early|"),
               "turning lsp off before initialize keeps the stderr written so far");
    }
}

// end の来ない進み具合は、その席が止まった・死んだときに消える（#1950 のレビュー L5）。
void verify_progress_forgotten_on_stop()
{
    Editing editing(lsp_settings(true));
    running(editing);
    static_cast<void>(delivered(
        editing, app::ServerMessage{progress_body("1", R"({"kind":"begin","title":"stuck"})")}));
    expect(status_of(editing.controller().frame()) == "cpp: stuck", "the begin shows");
    static_cast<void>(run_ex(editing.controller(), "set nolsp"));
    auto &server = editing.language_server();
    static_cast<void>(
        delivered(editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" +
                                              std::to_string(server.requests("shutdown").back()) +
                                              R"(,"result":null})"}));
    static_cast<void>(run_ex(editing.controller(), "set lsp"));
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    const auto again = delivered(
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" +
                                    std::to_string(server.requests("initialize").back()) +
                                    R"(,"result":{"capabilities":{"textDocumentSync":2}}})"});
    expect(status_of(again) == "<none>" && again.language_server_served,
           "a begin without an end is forgotten when its server stops");
}

// lsp がオフなら何も動かず、記録は空と 1 行知らせる（追補）。
void verify_log_when_off()
{
    Editing editing(lsp_settings(false));
    opened(editing);
    const std::size_t tabs = editing.controller().frame().tabs.size();
    const auto frame = editing.controller().apply_frame(app::OpenLanguageServerLog{});
    expect(editing.language_server().touches() == 0 && frame.tabs.size() == tabs &&
               message_of(frame) == "言語サーバーの記録はまだありません" &&
               status_of(frame) == "<none>",
           "with lsp off the log is empty, nothing is opened and the port is not touched");
}

// 言語サーバーの受けている文書の frame の列で、右の項目の配置が動かない（#1950 のレビュー R3）。
void expect_steady_items(std::initializer_list<const app::EditorFrame *> frames)
{
    const auto items_of = [](const app::EditorFrame &frame)
    {
        return core::status_bar_layout(
                   1280, 800, 96,
                   core::status_server_item(frame.diagnostic_counts, frame.language_server_served))
            .items;
    };
    const auto &front = **frames.begin();
    bool steady = true;
    for (const app::EditorFrame *frame : frames)
    {
        steady = steady && frame->language_server_served && items_of(*frame) == items_of(front);
    }
    expect(steady, "the right items do not move while progress comes and goes");
}

// 起動中と `$/progress`（追補）。
void verify_progress_status()
{
    Editing editing(lsp_settings(true));
    opened(editing);
    expect(status_of(editing.controller().frame()) == "cpp: 起動中",
           "the status shows starting before the server answers initialize");
    const auto started = delivered(editing, app::ServerStarted{});
    const auto &server = editing.language_server();
    expect(status_of(started) == "cpp: 起動中" &&
               contains(server.sent().back(), R"("window":{"workDoneProgress":true})"),
           "initialize offers workDoneProgress and the status still shows starting");
    const auto ready = delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2}}})"});
    expect(status_of(ready) == "<none>", "the running server without progress shows nothing");
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":"w1","method":"window/workDoneProgress/create","params":{"token":"idx"}})"}));
    expect(server.sent().back() == R"({"jsonrpc":"2.0","id":"w1","result":null})",
           "workDoneProgress/create is accepted with result null");
    const auto begun = delivered(
        editing, app::ServerMessage{progress_body(
                     R"("idx")", R"({"kind":"begin","title":"indexing","percentage":0})")});
    expect(status_of(begun) == "cpp: indexing 0%", "begin shows the title and the percentage");
    const auto reported = delivered(
        editing, app::ServerMessage{progress_body(
                     R"("idx")", R"({"kind":"report","message":"3/9","percentage":33})")});
    expect(status_of(reported) == "cpp: indexing 3/9 33%",
           "report keeps the title and updates the message and the percentage");
    const auto other = delivered(
        editing, app::ServerMessage{progress_body("2", R"({"kind":"begin","title":"loading"})")});
    expect(status_of(other) == "cpp: loading", "the latest moved token is shown");
    const auto back = delivered(editing, app::ServerMessage{progress_body(
                                             R"("idx")", R"({"kind":"report","percentage":50})")});
    expect(status_of(back) == "cpp: indexing 3/9 50%", "a report moves its token to the front");
    const auto ended =
        delivered(editing, app::ServerMessage{progress_body(R"("idx")", R"({"kind":"end"})")});
    expect(status_of(ended) == "cpp: loading", "end removes its token and the other one shows");
    const auto stray = delivered(
        editing, app::ServerMessage{progress_body("99", R"({"kind":"report","message":"x"})")});
    expect(status_of(stray) == "cpp: loading", "a report for an unknown token is ignored");
    const auto closed =
        delivered(editing, app::ServerMessage{progress_body("2", R"({"kind":"end"})")});
    expect(status_of(closed) == "<none>", "nothing shows when every token has ended");
    expect_steady_items({&started, &ready, &begun, &closed});
    static_cast<void>(delivered(
        editing, app::ServerMessage{progress_body("3", R"({"kind":"begin","title":"again"})")}));
    const auto restarted = delivered(editing, app::ServerExited{1});
    expect(status_of(restarted) == "cpp: 起動中",
           "a crashed server forgets its progress and the restart shows starting");
}
} // namespace

void verify_language_server_log()
{
    verify_server_log_line();
    verify_log_ring();
    verify_logged_messages();
    verify_progress_values();
    verify_status_item();
    verify_ex_lsp_log();
    verify_open_log();
    verify_log_when_off();
    verify_progress_status();
    verify_stop_keeps_unread_log();
    verify_progress_forgotten_on_stop();
}
} // namespace nenenib::tests
