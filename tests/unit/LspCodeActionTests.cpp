// scope `--lsp-code-action` の契約（#2005・ADR 0194 の S7 b）。クイックフィックス: core の読み
// （Command と CodeAction・isPreferred・data・edit と command の両方・崩れた形）、診断の code
// の型と data の往復（publish → 値 → context.diagnostics で同じ JSON）、controller の道（Ctrl+P の
// `=` の 候補・空の 1 行・控えと控えを捨てる・edit → command の順・resolve・`workspace/applyEdit`
// は頼んだ executeCommand の答えを待つ間だけ・入力の途中と頼んでいないときは applied:
// false・待ちは答えと 古さと席の止まりで必ず下りる）、鍵（Ctrl+.・`gra`・VISUAL
// の範囲）と能力を替え玉 ScriptedLanguageServer で確かめる。
#include "ActivateCommandChoice.hpp"
#include "CancelCommand.hpp"
#include "CloseTab.hpp"
#include "CommandChoice.hpp"
#include "CommandText.hpp"
#include "Diagnostic.hpp"
#include "DiagnosticCodeForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "JsonWrite.hpp"
#include "KeyChord.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerTick.hpp"
#include "LineNumber.hpp"
#include "LspAnswerFailure.hpp"
#include "LspCodeActions.hpp"
#include "LspDiagnostics.hpp"
#include "LspJsonRead.hpp"
#include "LspNotification.hpp"
#include "LspPositionEncoding.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "PlaceCaret.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimKey.hpp"
#include "VimLanguageQuery.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WindowDeactivated.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::string_view main_path = "C:\\work\\main.cpp";
constexpr std::string_view other_path = "C:\\work\\other.cpp";
constexpr std::string_view main_uri = "file:///C:/work/main.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view main_text = "int value = 1\nint main() { return value; }\n";
constexpr std::string_view fixed_text = "int value = 1;\nint main() { return value; }\n";
constexpr std::string_view renamed_text = "int count = 1\nint main() { return value; }\n";
constexpr std::string_view able =
    R"({"textDocumentSync":2,"codeActionProvider":{"resolveProvider":true},)"
    R"("executeCommandProvider":{"commands":["clangd.applyFix","fix.rename"]}})";
constexpr std::string_view action_method = "textDocument/codeAction";
constexpr std::string_view resolve_method = "codeAction/resolve";
constexpr std::string_view execute_method = "workspace/executeCommand";
// 1 行目の末尾の `;` の診断（文字列の code と data）と、2 行目の数の code の診断。
constexpr std::string_view semi_diagnostic =
    R"({"range":{"start":{"line":0,"character":12},"end":{"line":0,"character":13}},)"
    R"("severity":1,"code":"expected_semi","source":"clang","message":"expected ';'",)"
    R"("data":{"fixes":[1,"two",{"three":null}]}})";
constexpr std::string_view numbered_diagnostic =
    R"({"range":{"start":{"line":1,"character":20},"end":{"line":1,"character":25}},)"
    R"("severity":2,"code":2304,"message":"unused"})";
// 1 行目の末尾へ `;` を足す編集（changes の形）と、1 行目の value を count にする編集。
constexpr std::string_view semi_edit =
    R"({"changes":{"file:///C:/work/main.cpp":[{"range":{"start":{"line":0,"character":13},)"
    R"("end":{"line":0,"character":13}},"newText":";"}]}})";
constexpr std::string_view rename_edit =
    R"({"changes":{"file:///C:/work/main.cpp":[{"range":{"start":{"line":0,"character":4},)"
    R"("end":{"line":0,"character":9}},"newText":"count"}]}})";

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

[[nodiscard]] SettingsReading lsp_on()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

app::EditorFrame delivered(Editing &editing, app::ServerEvent event)
{
    editing.language_server().serve(std::move(event));
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] core::FilePath path_of(std::string_view path)
{
    return core::FilePath::parse(path).value();
}

[[nodiscard]] std::string notice(Editing &editing, core::UiMessage message)
{
    return std::string(core::ui_text(message, editing.controller().display_language()));
}

// main.cpp（と、two なら先に other.cpp）を開き、capabilities で `initialize` に答え、2 つの診断を
// 届けてからキャレットを 1 行目の `1` に置く。
void started(Editing &editing, std::string_view capabilities, bool two = false)
{
    editing.files().hold_at(std::string(main_path), Bytes{std::string(main_text)});
    editing.files().hold_at(std::string(other_path), Bytes{std::string("int other;\n")});
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    if (two)
    {
        applied(editing.controller(), app::OpenDocument{path_of(other_path)});
    }
    applied(editing.controller(), app::OpenDocument{path_of(main_path)});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                           std::string{capabilities} + "}}"}));
    editing.language_server().publish(main_uri, std::nullopt,
                                      "[" + std::string{semi_diagnostic} + "," +
                                          std::string{numbered_diagnostic} + "]");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{13}},
                            core::SelectionAnchoring::collapse});
}

app::EditorFrame asked(Editing &editing)
{
    return editing.controller().apply_frame(
        app::QueryLanguage{core::LanguageQueryKind::code_action});
}

app::EditorFrame answered(Editing &editing, std::string_view method, std::string_view result)
{
    const auto ids = editing.language_server().requests(method);
    editing.language_server().answer(ids.empty() ? -1 : ids.back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

// 誤りで答える。
app::EditorFrame failed(Editing &editing, std::string_view method, std::string_view message)
{
    const auto ids = editing.language_server().requests(method);
    editing.language_server().serve(app::ServerMessage{
        R"({"jsonrpc":"2.0","id":)" + std::to_string(ids.empty() ? -1 : ids.back()) +
        R"(,"error":{"code":-32603,"message":")" + std::string{message} + R"("}})"});
    return editing.controller().apply_frame(app::WorkCompleted{});
}

// サーバーからの `workspace/applyEdit`（id は文字列）。
app::EditorFrame apply_edit_sent(Editing &editing, std::string_view id, std::string_view edit)
{
    return delivered(editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":")" + std::string{id} +
                                                 R"(","method":"workspace/applyEdit",)"
                                                 R"("params":{"label":"fix","edit":)" +
                                                 std::string{edit} + "}}"});
}

// 送った本文のうち、文字列の id の答え（無ければ空）。
[[nodiscard]] std::string reply_to(Editing &editing, std::string_view id)
{
    const std::string key = R"("id":")" + std::string{id} + "\"";
    for (const std::string &body : editing.language_server().sent())
    {
        if (contains(body, key) && contains(body, R"("result")"))
        {
            return body;
        }
    }
    return {};
}

[[nodiscard]] std::size_t sent_index(Editing &editing, std::string_view part)
{
    const auto &sent = editing.language_server().sent();
    for (std::size_t index = sent.size(); index > 0; --index)
    {
        if (contains(sent.at(index - 1), part))
        {
            return index - 1;
        }
    }
    return sent.size();
}

// 好んで選ぶ quickfix の edit・Command の literal・edit と command の両方・data だけの refactor。
[[nodiscard]] std::string four_actions()
{
    return R"([{"title":"Insert ';'","kind":"quickfix","isPreferred":true,"edit":)" +
           std::string{semi_edit} +
           R"(},{"title":"Apply fix","command":"clangd.applyFix","arguments":[{"n":1}]},)"
           R"({"title":"Rename to count","kind":"refactor.rewrite","edit":)" +
           std::string{rename_edit} +
           R"(,"command":{"title":"after","command":"fix.rename","arguments":[2]}},)"
           R"({"title":"Extract","kind":"refactor.extract","data":{"id":7}}])";
}

[[nodiscard]] std::vector<core::LspCodeAction> read(std::string_view json)
{
    const auto parsed = core::parse_json(json);
    if (!parsed.has_value())
    {
        return {};
    }
    auto actions = core::lsp_code_actions(parsed.value());
    return actions.has_value() ? std::move(actions).value() : std::vector<core::LspCodeAction>{};
}

[[nodiscard]] core::VimStep vim_stepped(std::u32string_view keys)
{
    const auto buffer = core::TextBuffer::from_utf8("ab\ncd").value();
    const core::VimEditorView view{buffer, core::collapsed_at(core::Offset{0}),
                                   core::VimViewport{core::LineNumber{1}, 8, 0}};
    core::VimState state = empty_vim_state();
    core::VimStep step{state, core::VimNoEffect{}};
    for (const char32_t key : keys)
    {
        step = core::vim_step(state, view, core::VimKey{core::VimCharacter{key}});
        state = step.next;
    }
    return step;
}

[[nodiscard]] std::optional<core::LanguageQueryKind> vim_queried(std::u32string_view keys)
{
    const core::VimStep step = vim_stepped(keys);
    const auto *found = std::get_if<core::VimLanguageQuery>(&step.effect);
    return found != nullptr ? std::optional{found->kind} : std::nullopt;
}

// ---------------------------------------------------------------- core の読み

void verify_reading()
{
    const auto actions = read(four_actions());
    expect(actions.size() == 4, "every readable item is kept in the server's order");
    if (actions.size() != 4)
    {
        return;
    }
    expect(actions.at(0).title == "Insert ';'" && actions.at(0).kind == "quickfix" &&
               actions.at(0).preferred && actions.at(0).edit.has_value() &&
               !actions.at(0).command.has_value() && !actions.at(0).resolvable,
           "a CodeAction reads its title, kind, isPreferred and edit");
    expect(actions.at(1).title == "Apply fix" && actions.at(1).kind.empty() &&
               !actions.at(1).edit.has_value() && actions.at(1).command.has_value() &&
               actions.at(1).command.value_or(core::LspCommand{}).command == "clangd.applyFix" &&
               actions.at(1).command.value_or(core::LspCommand{}).arguments.has_value(),
           "a Command literal is an item whose command is itself");
    expect(actions.at(2).edit.has_value() && actions.at(2).command.has_value() &&
               actions.at(2).command.value_or(core::LspCommand{}).command == "fix.rename",
           "an item can have both an edit and a command");
    expect(actions.at(3).resolvable && !actions.at(3).edit.has_value() &&
               !actions.at(3).command.has_value(),
           "an item with data only is resolved later");
    expect(
        read(R"([{"kind":"quickfix","edit":{}},{"title":"x","disabled":{"reason":"no"},"edit":{}},)"
             R"({"title":"bare"},{"title":"y","command":3},7,{"title":"ok","edit":{}}])")
                .size() == 1,
        "items without a title, disabled items, items with nothing to do and other shapes are "
        "left out one by one");
    const auto null = core::parse_json("null");
    const auto object = core::parse_json("{}");
    expect(null.has_value() && core::lsp_code_actions(null.value()).has_value() &&
               core::lsp_code_actions(null.value()).value().empty() && object.has_value() &&
               core::lsp_code_actions(object.value()).error() ==
                   core::LspAnswerFailure::invalid_result,
           "null is no items and another shape fails");
    std::string many = "[";
    for (std::size_t index = 0; index < core::lsp_code_action_limit + 3; ++index)
    {
        many += std::string{index == 0 ? "" : ","} + R"({"title":"t","edit":{}})";
    }
    expect(read(many + "]").size() == core::lsp_code_action_limit, "the items stop at the limit");
    const auto params = core::write_json(core::lsp_execute_command_params(
        actions.at(1).command.value_or(core::LspCommand{"", "", std::nullopt})));
    expect(params == R"({"command":"clangd.applyFix","arguments":[{"n":1}]})",
           "executeCommand passes the command and its arguments as they came");
}

// publish → 値 → context.diagnostics で同じ JSON（code の型と data）。
void verify_diagnostic_round_trip()
{
    const auto text = core::TextBuffer::from_utf8(main_text).value();
    const auto params =
        core::parse_json(R"({"uri":"u","diagnostics":[)" + std::string{semi_diagnostic} + "," +
                         std::string{numbered_diagnostic} + "]}");
    const auto published = core::lsp_published_diagnostics(
        core::LspNotification{.method = std::string{core::lsp_publish_diagnostics_method},
                              .params = params.value_or(core::JsonValue{})},
        text, core::LspPositionEncoding::utf16);
    if (!published.has_value() || published.value().diagnostics.size() != 2)
    {
        expect(false, "both diagnostics are read");
        return;
    }
    const auto &read = published.value().diagnostics;
    expect(read.at(0).code == "expected_semi" &&
               read.at(0).code_form == core::DiagnosticCodeForm::text &&
               read.at(0).data.has_value() && read.at(1).code == "2304" &&
               read.at(1).code_form == core::DiagnosticCodeForm::number &&
               !read.at(1).data.has_value(),
           "a diagnostic keeps the JSON type of its code and its data");
    expect(core::lsp_diagnostic_json(read.at(0), text, core::LspPositionEncoding::utf16) ==
                   core::parse_json(semi_diagnostic).value_or(core::JsonValue{}) &&
               core::lsp_diagnostic_json(read.at(1), text, core::LspPositionEncoding::utf16) ==
                   core::parse_json(numbered_diagnostic).value_or(core::JsonValue{}),
           "a diagnostic goes back to the server as the JSON it came in");
    const std::string large(core::lsp_diagnostic_data_limit, 'x');
    const auto big = core::parse_json(
        R"({"uri":"u","diagnostics":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},"message":"m","code":1.5,"data":")" +
        large + R"("}]})");
    const auto dropped = core::lsp_published_diagnostics(
        core::LspNotification{.method = std::string{core::lsp_publish_diagnostics_method},
                              .params = big.value_or(core::JsonValue{})},
        text, core::LspPositionEncoding::utf16);
    expect(dropped.has_value() && dropped.value().diagnostics.size() == 1 &&
               !dropped.value().diagnostics.front().data.has_value() &&
               dropped.value().diagnostics.front().code_form == core::DiagnosticCodeForm::none,
           "data over the limit and a code that is not an integer or a string are not kept");
}

// ---------------------------------------------------------------- 鍵と能力

void verify_keys()
{
    const core::KeyChord period{true, false, core::OperationKey::period};
    expect(
        core::operation_for(period, core::EditMode::ordinary) == core::EditorOperation::quick_fix &&
            core::operation_for(period, core::EditMode::vim) == core::EditorOperation::quick_fix &&
            core::key_chord_label(period) == "Ctrl+.",
        "Ctrl+. asks for quick fixes in both modes");
    expect(vim_queried(U"gra") == core::LanguageQueryKind::code_action &&
               vim_queried(U"grn") == core::LanguageQueryKind::rename &&
               vim_stepped(U"gra").failure == core::VimRepeatFailure::refused &&
               !vim_queried(U"dgra").has_value() && !vim_queried(U"2gra").has_value() &&
               !vim_queried(U"grx").has_value(),
           "gra asks for quick fixes on the refused step of a key without a row like grr");
}

void verify_capabilities()
{
    Editing editing(lsp_on());
    started(editing, able);
    const std::string &initialize = editing.language_server().sent().front();
    expect(contains(initialize,
                    R"("codeAction":{"codeActionLiteralSupport":{"codeActionKind":{"valueSet":)"
                    R"(["","quickfix","refactor","refactor.extract","refactor.inline",)"
                    R"("refactor.rewrite","source","source.organizeImports","source.fixAll"]}},)"
                    R"("isPreferredSupport":true,"dataSupport":true,)"
                    R"("resolveSupport":{"properties":["edit"]}})") &&
               contains(initialize, R"("applyEdit":true)") &&
               contains(initialize, R"("executeCommand":{})") &&
               !contains(initialize, "disabledSupport"),
           "initialize offers code action literals, isPreferred, data, resolve, applyEdit and "
           "executeCommand, but not disabledSupport");
    const auto &abilities = editing.controller().language_server_abilities();
    expect(abilities.code_action && abilities.code_action_resolve &&
               abilities.execute_commands ==
                   std::vector<std::string>{"clangd.applyFix", "fix.rename"},
           "codeActionProvider, resolveProvider and the commands are read");
    Editing none(lsp_on());
    started(none, R"({"textDocumentSync":2})");
    const auto refused = asked(none);
    expect(message_of(refused) == notice(none, core::UiMessage::notice_code_action_not_supported) &&
               none.language_server().requests(action_method).empty() &&
               !refused.command_palette.has_value(),
           "a server without codeActionProvider is not asked and says so");
}

// ---------------------------------------------------------------- controller の道

// 頼む形: 範囲はキャレット、context は範囲に掛かる診断を受けた JSON のまま・Invoked。
void verify_request()
{
    Editing editing(lsp_on());
    started(editing, able);
    static_cast<void>(asked(editing));
    const auto &sent = editing.language_server().sent();
    expect(
        contains(sent.back(), R"("method":"textDocument/codeAction")") &&
            contains(
                sent.back(),
                R"("range":{"start":{"line":0,"character":12},"end":{"line":0,"character":12}})") &&
            contains(sent.back(), R"("context":{"diagnostics":[)" + std::string{semi_diagnostic} +
                                      R"(],"triggerKind":1})"),
        "the request is the caret with the diagnostics on it as they came, invoked by hand");
}

// 答えは `=` の一覧になり、題名は title、補足は kind（好んで選ぶ項目は印）。edit を選ぶと当たる。
void verify_list_and_edit()
{
    Editing editing(lsp_on());
    started(editing, able);
    static_cast<void>(asked(editing));
    const auto listed = answered(editing, action_method, four_actions());
    if (!listed.command_palette.has_value() || listed.command_palette.value().rows.size() != 4)
    {
        expect(false, "the answer opens the = list with every item");
        return;
    }
    const auto &rows = listed.command_palette.value().rows;
    const auto detail = [](const auto &row)
    { return row.detail.has_value() ? std::string{row.detail.value().text()} : std::string{}; };
    expect(rows.at(0).label.text() == "Insert ';'" && detail(rows.at(0)) == "* quickfix" &&
               rows.at(1).label.text() == "Apply fix" && detail(rows.at(1)).empty() &&
               detail(rows.at(2)) == "refactor.rewrite",
           "rows are the titles with the kind and a mark on the preferred item");
    const auto chosen = editing.controller().apply_frame(app::ActivateCommandChoice{0});
    expect(!chosen.command_palette.has_value() &&
               whole_vim_body(editing.controller()) == fixed_text &&
               message_of(chosen) ==
                   core::ui_text_filled(core::UiMessage::notice_server_edit_applied,
                                        editing.controller().display_language(), {"1"}),
           "choosing an item with an edit applies it through the one path");
}

// edit と command の両方: edit を当て、`didChange` を流してから命令を送る。
void verify_edit_then_command()
{
    Editing editing(lsp_on());
    started(editing, able);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, action_method, four_actions()));
    static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{2}));
    const std::size_t change = sent_index(editing, R"("method":"textDocument/didChange")");
    const std::size_t command = sent_index(editing, R"("method":"workspace/executeCommand")");
    expect(whole_vim_body(editing.controller()) == renamed_text &&
               command < editing.language_server().sent().size() && change < command &&
               contains(editing.language_server().sent().at(command),
                        R"("params":{"command":"fix.rename","arguments":[2]})"),
           "the edit is applied first, its didChange goes out, then the command is sent");
}

// data だけの項目は resolve で edit を引いて当てる。
void verify_resolve()
{
    Editing editing(lsp_on());
    started(editing, able);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, action_method, four_actions()));
    static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{3}));
    const auto &sent = editing.language_server().sent();
    expect(contains(sent.back(), R"("method":"codeAction/resolve")") &&
               contains(sent.back(), R"("params":{"title":"Extract","kind":"refactor.extract",)"
                                     R"("data":{"id":7}})"),
           "an item without an edit is resolved with the item as it came");
    static_cast<void>(answered(editing, resolve_method,
                               R"({"title":"Extract","kind":"refactor.extract","edit":)" +
                                   std::string{semi_edit} + "}"));
    expect(whole_vim_body(editing.controller()) == fixed_text, "the resolved edit is applied");

    Editing plain(lsp_on());
    started(plain, R"({"textDocumentSync":2,"codeActionProvider":true})");
    static_cast<void>(asked(plain));
    static_cast<void>(answered(plain, action_method, four_actions()));
    const std::size_t before = plain.language_server().sent().size();
    static_cast<void>(plain.controller().apply_frame(app::ActivateCommandChoice{3}));
    expect(plain.language_server().sent().size() == before &&
               whole_vim_body(plain.controller()) == main_text,
           "a server without resolveProvider is not asked to resolve");
}

// 頼んだ命令の答えを待つ間の `workspace/applyEdit` は当てて applied: true、答えの後は false。
void verify_apply_edit_while_awaited()
{
    Editing editing(lsp_on());
    started(editing, able);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, action_method, four_actions()));
    static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{1}));
    expect(contains(editing.language_server().sent().back(),
                    R"("params":{"command":"clangd.applyFix","arguments":[{"n":1}]})"),
           "a Command item sends the command");
    const auto applied_frame = apply_edit_sent(editing, "s1", semi_edit);
    expect(reply_to(editing, "s1") == R"({"jsonrpc":"2.0","id":"s1","result":{"applied":true}})" &&
               whole_vim_body(editing.controller()) == fixed_text &&
               message_of(applied_frame) ==
                   core::ui_text_filled(core::UiMessage::notice_server_edit_applied,
                                        editing.controller().display_language(), {"1"}),
           "an applyEdit while the command is awaited is applied");
    static_cast<void>(apply_edit_sent(editing, "s3", rename_edit));
    expect(contains(reply_to(editing, "s3"), R"("applied":true)") &&
               whole_vim_body(editing.controller()) ==
                   "int count = 1;\nint main() { return value; }\n",
           "a second applyEdit of the same command is checked against the text after the first");
    static_cast<void>(answered(editing, execute_method, "null"));
    static_cast<void>(apply_edit_sent(editing, "s2", semi_edit));
    expect(contains(reply_to(editing, "s2"), R"("applied":false,"failureReason":")") &&
               whole_vim_body(editing.controller()) ==
                   "int count = 1;\nint main() { return value; }\n",
           "after the command's answer an applyEdit is refused");
}

// Command の項目を選び、命令の答えを待つ所まで進める。
void awaiting(Editing &editing)
{
    started(editing, able);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, action_method, four_actions()));
    static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{1}));
}

// 頼んでいない applyEdit・入力の途中・誤りの答えと古さで下りた待ち・照合が断る applyEdit。
void verify_apply_edit_refusals()
{
    Editing unasked(lsp_on());
    started(unasked, able);
    static_cast<void>(apply_edit_sent(unasked, "u1", semi_edit));
    expect(
        contains(
            reply_to(unasked, "u1"),
            R"("applied":false,"failureReason":"Nib is not waiting for a workspace/executeCommand")") &&
            whole_vim_body(unasked.controller()) == main_text,
        "an applyEdit nobody asked for is refused and changes nothing");

    Editing busy(lsp_on());
    awaiting(busy);
    static_cast<void>(busy.controller().apply(app::OpenCommandPalette{}));
    const auto refused = apply_edit_sent(busy, "b1", semi_edit);
    expect(contains(reply_to(busy, "b1"),
                    R"("applied":false,"failureReason":"the editor is in the middle of input")") &&
               whole_vim_body(busy.controller()) == main_text &&
               message_of(refused) == notice(busy, core::UiMessage::notice_server_edit_busy),
           "an applyEdit in the middle of input is refused without waiting");

    Editing errored(lsp_on());
    awaiting(errored);
    const auto failure = failed(errored, execute_method, "no fix here");
    static_cast<void>(apply_edit_sent(errored, "e1", semi_edit));
    expect(message_of(failure) == "no fix here" &&
               contains(reply_to(errored, "e1"), R"("applied":false)") &&
               whole_vim_body(errored.controller()) == main_text,
           "an error answer says the server's message and lowers the wait");
}

// 古さで下りた待ち・本文が動いた後・resource operation の applyEdit。
void verify_apply_edit_lowered()
{
    Editing aged(lsp_on());
    awaiting(aged);
    for (std::size_t tick = 0; tick < app::language_server_stale_ticks; ++tick)
    {
        static_cast<void>(aged.controller().apply(app::LanguageServerTick{}));
    }
    static_cast<void>(apply_edit_sent(aged, "t1", semi_edit));
    expect(contains(reply_to(aged, "t1"), R"("applied":false)") &&
               whole_vim_body(aged.controller()) == main_text,
           "a command that never answers stops being awaited when it grows stale");

    Editing moved(lsp_on());
    awaiting(moved);
    applied(moved.controller(), app::InsertText{"x"});
    static_cast<void>(apply_edit_sent(moved, "m1", semi_edit));
    expect(contains(reply_to(moved, "m1"), R"("applied":false)") &&
               !contains(whole_vim_body(moved.controller()), "1;"),
           "an applyEdit after the text moved since the command was sent is refused");

    Editing broken(lsp_on());
    awaiting(broken);
    static_cast<void>(apply_edit_sent(
        broken, "r1",
        R"({"documentChanges":[{"kind":"create","uri":"file:///C:/work/new.cpp"}]})"));
    expect(contains(reply_to(broken, "r1"),
                    R"("failureReason":"resource operations are not supported")"),
           "an applyEdit with a resource operation is refused with the reason");
}

// 空の答え・古い答え・本文が変わった後の `=`・Vim の INSERT・控えと控えを捨てる。
void verify_empty_stale_and_held()
{
    Editing editing(lsp_on());
    started(editing, able);
    static_cast<void>(asked(editing));
    const auto empty = answered(editing, action_method, "[]");
    expect(!empty.command_palette.has_value() &&
               message_of(empty) == notice(editing, core::UiMessage::notice_code_actions_not_found),
           "an empty answer opens nothing and says so");
    static_cast<void>(asked(editing));
    applied(editing.controller(), app::InsertText{"x"});
    expect(!answered(editing, action_method, four_actions()).command_palette.has_value(),
           "an answer for an older revision is dropped");

    Editing held(lsp_on());
    started(held, able, true);
    static_cast<void>(asked(held));
    static_cast<void>(held.controller().apply(app::WindowDeactivated{}));
    expect(!answered(held, action_method, four_actions()).command_palette.has_value(),
           "an answer while the window is inactive is held without opening the list");
    static_cast<void>(held.controller().apply(app::OpenCommandPalette{}));
    const auto typed = held.controller().apply_frame(app::CommandText{"="});
    expect(typed.command_palette.has_value() && typed.command_palette.value().rows.size() == 4,
           "typing = shows the held items");
    static_cast<void>(held.controller().apply(app::CancelCommand{}));
    applied(held.controller(), app::InsertText{"y"});
    static_cast<void>(held.controller().apply(app::OpenCommandPalette{}));
    const auto moved = held.controller().apply_frame(app::CommandText{"="});
    const std::string expected_line =
        core::ui_text_filled(core::UiMessage::palette_empty_code_actions,
                             held.controller().display_language(), {"Ctrl+.", "gra"});
    expect(moved.command_palette.has_value() && moved.command_palette.value().rows.empty() &&
               moved.command_palette.value().empty_line.has_value() &&
               moved.command_palette.value().empty_line.value().text() == expected_line,
           "after the text changes the held items are not shown and the list says how to ask");
    static_cast<void>(held.controller().apply(app::CancelCommand{}));
    static_cast<void>(asked(held));
    static_cast<void>(held.controller().apply(app::WindowDeactivated{}));
    static_cast<void>(answered(held, action_method, four_actions()));
    applied(held.controller(), app::CloseTab{1});
    applied(held.controller(), app::OpenDocument{path_of(main_path)});
    static_cast<void>(held.controller().apply(app::OpenCommandPalette{}));
    const auto forgotten = held.controller().apply_frame(app::CommandText{"="});
    expect(forgotten.command_palette.has_value() && forgotten.command_palette.value().rows.empty(),
           "after closing the asked document no held items are shown");

    Editing insert(lsp_on());
    started(insert, able);
    applied(insert.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(insert.controller(), "i");
    const auto refused = asked(insert);
    expect(message_of(refused) == notice(insert, core::UiMessage::notice_code_action_in_insert) &&
               insert.language_server().requests(action_method).empty(),
           "a quick fix in Insert mode is refused before asking");
}

// Vim の `gra` と VISUAL の範囲（Ctrl+. で選択の範囲を頼み、当てたら NORMAL へ戻る）。
void verify_vim()
{
    Editing editing(lsp_on());
    started(editing, able);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "gra");
    expect(editing.language_server().requests(action_method).size() == 1, "gra asks for fixes");
    const auto opened = answered(editing, action_method, four_actions());
    expect(opened.command_palette.has_value(), "the answer opens the = list in vim");
    static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{0}));
    expect(whole_vim_body(editing.controller()) == fixed_text, "gra fixes in vim");

    Editing visual(lsp_on());
    started(visual, able);
    applied(visual.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(visual.controller(), "0vll");
    static_cast<void>(asked(visual));
    expect(contains(visual.language_server().sent().back(),
                    R"("range":{"start":{"line":0,"character":0},"end":{"line":0,"character":3}})"),
           "VISUAL asks for the drawn selection");
    static_cast<void>(answered(visual, action_method, four_actions()));
    static_cast<void>(visual.controller().apply_frame(app::ActivateCommandChoice{0}));
    expect(whole_vim_body(visual.controller()) == fixed_text &&
               visual.controller().frame().vim_mode == core::VimMode::normal,
           "the fix chosen from VISUAL leaves VISUAL");
}
// 実在の言語サーバーの答え（#2005 の実測・D:/NeNeNib/scripts/20261124/s7b/s7b-nib-probe.py が Nib
// の 能力で頼んで書き出した答えのまま）。調べは `didOpen` を version 7 で送ったので、Nib が送る版
// （改訂番号 0）に読み替えてから届ける。
// 言語・パス・URI・本文・能力・診断・答えの項目・当てた後の本文・送る命令（空なら送らない）・
// resolve の答え（空なら頼まない）。
using RealAnswer =
    std::tuple<std::string_view, std::string_view, std::string_view, std::string_view,
               std::string_view, std::string_view, std::string_view, std::string_view,
               std::string_view, std::string_view>;

[[nodiscard]] std::string at_revision_zero(std::string_view json)
{
    std::string replaced{json};
    for (std::size_t at = replaced.find(R"("version":7)"); at != std::string::npos;
         at = replaced.find(R"("version":7)", at))
    {
        replaced.replace(at, std::string_view{R"("version":7)"}.size(), R"("version":0)");
    }
    return replaced;
}

constexpr std::array<RealAnswer, 4> real_answers{{
    // clangd 19.1.5: `edit` が直接（documentChanges@v7）・isPreferred。
    {"cpp", R"j(D:\NeNeNib\scripts\20261124\s7-probe\cpp\src\main.cpp)j",
     "file:///D:/NeNeNib/scripts/20261124/s7-probe/cpp/src/main.cpp",
     "#include \"lib.h\"\nint main() {\n    int total = greet_count(2)\n    return total;\n}\n",
     R"j({"textDocumentSync":2,"codeActionProvider":{"codeActionKinds":["quickfix","refactor","info"]},"executeCommandProvider":{"commands":["clangd.applyFix","clangd.applyTweak"]}})j",
     R"j([{"code":"expected_semi_declaration","message":"Expected ';' at end of declaration (fix available)","range":{"end":{"character":10,"line":3},"start":{"character":4,"line":3}},"severity":1,"source":"clang"}])j",
     R"j({"edit":{"documentChanges":[{"edits":[{"newText":";","range":{"end":{"character":30,"line":2},"start":{"character":30,"line":2}}}],"textDocument":{"uri":"file:///D:/NeNeNib/scripts/20261124/s7-probe/cpp/src/main.cpp","version":7}}]},"isPreferred":true,"kind":"quickfix","title":"insert ';'"})j",
     "#include \"lib.h\"\nint main() {\n    int total = greet_count(2);\n    return total;\n}\n",
     "", ""},
    // typescript-language-server: `edit`（percent-encoded の URI）と `command` の両方・数の code。
    {"typescript", R"j(D:\NeNeNib\scripts\20261124\s7-probe\ts\src\main.ts)j",
     "file:///D:/NeNeNib/scripts/20261124/s7-probe/ts/src/main.ts",
     "import { greet } from \"./lib\";\n\nconsole.log(greet(\"world\"));\nconst unusedValue = "
     "greet;\nconsole.log(join(\"a\"));\n",
     R"j({"textDocumentSync":2,"codeActionProvider":{"resolveProvider":true},"executeCommandProvider":{"commands":["_typescript.applyRefactoring","_typescript.applyCodeActionCommand"]}})j",
     R"j([{"range":{"start":{"line":4,"character":12},"end":{"line":4,"character":16}},"message":"Cannot find name 'join'.","severity":1,"code":2304,"source":"typescript"}])j",
     R"j({"title":"Add import from \"./util\"","kind":"quickfix","command":{"command":"_typescript.applyCodeActionCommand","arguments":[{"documentUri":"file:///d%3A/NeNeNib/scripts/20261124/s7-probe/ts/src/main.ts"}],"title":""},"edit":{"documentChanges":[{"textDocument":{"uri":"file:///d%3A/NeNeNib/scripts/20261124/s7-probe/ts/src/main.ts","version":7},"edits":[{"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":0}},"newText":"import { join } from \"./util\";\n"}]}]},"isPreferred":true})j",
     "import { greet } from \"./lib\";\nimport { join } from \"./util\";\n\nconsole.log(greet("
     "\"world\"));\nconst unusedValue = greet;\nconsole.log(join(\"a\"));\n",
     "_typescript.applyCodeActionCommand", ""},
    // gopls: source.organizeImports の `edit` が直接（診断なし）。
    {"go", R"j(D:\NeNeNib\scripts\20261124\s7-probe\go\main.go)j",
     "file:///D:/NeNeNib/scripts/20261124/s7-probe/go/main.go",
     "package main\n\nimport (\n\t\"fmt\"\n\t\"os\"\n)\n\nfunc main() "
     "{\n\tfmt.Println(greet(\"world\"))\n}\n",
     R"j({"textDocumentSync":2,"codeActionProvider":{"resolveProvider":true},"executeCommandProvider":{"commands":["gopls.apply_fix"]}})j",
     "[]",
     R"j({"title":"Organize Imports","kind":"source.organizeImports","edit":{"documentChanges":[{"textDocument":{"version":7,"uri":"file:///D:/NeNeNib/scripts/20261124/s7-probe/go/main.go"},"edits":[{"range":{"start":{"line":3,"character":5},"end":{"line":4,"character":4}},"newText":""}]}]}})j",
     "package main\n\nimport (\n\t\"fmt\"\n)\n\nfunc main() "
     "{\n\tfmt.Println(greet(\"world\"))\n}\n",
     "", ""},
    // rust-analyzer: `data` だけの refactor を resolve で引く（小文字の d の URI・utf-8
    // の位置・data は 答えの id と version だけに縮めた）。診断は `tags` と `relatedInformation`
    // を持つ（Nib は戻さない）ので 届けない。
    {"rust", R"j(D:\NeNeNib\scripts\20261124\s7-probe\rust\src\main.rs)j",
     "file:///D:/NeNeNib/scripts/20261124/s7-probe/rust/src/main.rs",
     "mod lib_mod;\nuse lib_mod::greet;\n\nfn main() {\n    let unused_value = 3;\n    "
     "println!(\"{}\", "
     "greet(\"world\"));\n}\n",
     R"j({"positionEncoding":"utf-8","textDocumentSync":2,"codeActionProvider":{"resolveProvider":true}})j",
     "[]",
     R"j({"title":"Insert explicit type `i32`","kind":"refactor.rewrite","data":{"id":"add_explicit_type:RefactorRewrite:0:","version":7}})j",
     "mod lib_mod;\nuse lib_mod::greet;\n\nfn main() {\n    let unused_value: i32 = 3;\n    "
     "println!(\"{}\", greet(\"world\"));\n}\n",
     "",
     R"j({"title":"Insert explicit type `i32`","kind":"refactor.rewrite","edit":{"documentChanges":[{"textDocument":{"version":7,"uri":"file:///d:/NeNeNib/scripts/20261124/s7-probe/rust/src/main.rs"},"edits":[{"range":{"start":{"line":4,"character":20},"end":{"line":4,"character":20}},"newText":": i32"}]}]}})j"},
}};

// 鍵の並びを問わない JSON の比べ（object は同じ鍵の集合で値が同じ・配列は順に同じ）。
[[nodiscard]] bool same_json(const core::JsonValue &left, const core::JsonValue &right)
{
    const auto *left_object = std::get_if<core::JsonObject>(&left.value);
    const auto *right_object = std::get_if<core::JsonObject>(&right.value);
    if (left_object != nullptr && right_object != nullptr)
    {
        return left_object->size() == right_object->size() &&
               std::ranges::all_of(
                   *left_object,
                   [right_object](const auto &member)
                   {
                       const auto found = core::json_member(*right_object, member.first);
                       return found.has_value() && same_json(member.second, found.value().get());
                   });
    }
    const auto *left_list = std::get_if<core::JsonArray>(&left.value);
    const auto *right_list = std::get_if<core::JsonArray>(&right.value);
    if (left_list != nullptr && right_list != nullptr)
    {
        return std::ranges::equal(*left_list, *right_list,
                                  [](const auto &a, const auto &b) { return same_json(a, b); });
    }
    return left == right;
}

// 送った codeAction の本文の params.context.diagnostics（読めなければ null）。
[[nodiscard]] core::JsonValue sent_diagnostics(std::string_view body)
{
    const auto parsed = core::parse_json(body);
    const auto *message =
        parsed.has_value() ? std::get_if<core::JsonObject>(&parsed.value().value) : nullptr;
    const auto params = message != nullptr ? core::json_member(*message, "params") : std::nullopt;
    const auto *object =
        params.has_value() ? std::get_if<core::JsonObject>(&params.value().get().value) : nullptr;
    const auto context = object != nullptr ? core::json_member(*object, "context") : std::nullopt;
    const auto *inner =
        context.has_value() ? std::get_if<core::JsonObject>(&context.value().get().value) : nullptr;
    const auto listed = inner != nullptr ? core::json_member(*inner, "diagnostics") : std::nullopt;
    return listed.has_value() ? listed.value().get() : core::JsonValue{};
}

// キャレットを最初の診断の始まりに置く（診断が無ければ置かない・本文は ASCII なので桁は文字の数）。
void place_on_first(Editing &editing, std::string_view diagnostics)
{
    const auto parsed = core::parse_json(diagnostics);
    const auto *list =
        parsed.has_value() ? std::get_if<core::JsonArray>(&parsed.value().value) : nullptr;
    const auto *first =
        list != nullptr && !list->empty() ? core::json_object_of(list->front()) : nullptr;
    const auto range = first != nullptr ? core::lsp_range_member(*first, "range") : std::nullopt;
    if (!range.has_value())
    {
        return;
    }
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{range.value().start.line + 1},
                                               core::Column{range.value().start.character + 1}},
                            core::SelectionAnchoring::collapse});
}

void verify_real_answers()
{
    for (const auto &[language, path, uri, text, capabilities, diagnostics, action, fixed, command,
                      resolved] : real_answers)
    {
        auto settings = core::default_editor_settings();
        settings.lsp = true;
        settings.lsp_commands = {{std::string{language}, std::string{clangd_command}}};
        Editing editing(SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}});
        editing.files().hold_at(std::string(path), Bytes{std::string(text)});
        static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
        applied(editing.controller(), app::OpenDocument{path_of(path)});
        static_cast<void>(delivered(editing, app::ServerStarted{}));
        static_cast<void>(delivered(
            editing,
            app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                               std::string{capabilities} + "}}"}));
        editing.language_server().publish(uri, std::nullopt, diagnostics);
        static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
        place_on_first(editing, diagnostics);
        static_cast<void>(asked(editing));
        const bool returned = same_json(sent_diagnostics(editing.language_server().sent().back()),
                                        core::parse_json(diagnostics).value_or(core::JsonValue{}));
        const auto listed = answered(editing, action_method, "[" + at_revision_zero(action) + "]");
        static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{0}));
        if (!resolved.empty())
        {
            static_cast<void>(answered(editing, resolve_method, at_revision_zero(resolved)));
        }
        const bool commanded =
            command.empty() ||
            contains(editing.language_server().sent().back(),
                     R"("method":"workspace/executeCommand","params":{"command":")" +
                         std::string{command} + "\"");
        const std::string name{language};
        expect(returned, ("the diagnostics go back as they came (" + name + ")").c_str());
        expect(listed.command_palette.has_value(),
               ("the answer opens the list (" + name + ")").c_str());
        expect(whole_vim_body(editing.controller()) == fixed,
               ("a real server's quick fix applies in Nib (" + name + ")").c_str());
        expect(commanded, ("the command follows the edit (" + name + ")").c_str());
    }
}
} // namespace

void verify_lsp_code_action_scope()
{
    verify_reading();
    verify_diagnostic_round_trip();
    verify_keys();
    verify_capabilities();
    verify_request();
    verify_list_and_edit();
    verify_edit_then_command();
    verify_resolve();
    verify_apply_edit_while_awaited();
    verify_apply_edit_refusals();
    verify_apply_edit_lowered();
    verify_empty_stale_and_held();
    verify_vim();
    verify_real_answers();
}
} // namespace nenenib::tests
