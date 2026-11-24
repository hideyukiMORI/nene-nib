// scope `--lsp-rename` の契約（#2004・ADR 0194 の S7 a）。言語サーバーの編集（WorkspaceEdit）を
// 当てる土台と名前の変更: core の読み（両形・版・resource operation・注釈・上限・崩れた形と
// prepareRename の 3 形）、controller の道（検査で 1 つでも断れば 1 つも変えない・開いていない
// ファイルは固定のタブで未保存・文書ごとに 1 回の undo・頼んだタブへ戻る・使った順は動かさない）、
// 鍵（F2・`grn`・入力行）と能力を替え玉 ScriptedLanguageServer で確かめる。
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "CompletionCandidates.hpp"
#include "DocumentReadRequest.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InputLinePrompt.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "KeyChord.hpp"
#include "LanguageQueryKind.hpp"
#include "LineNumber.hpp"
#include "LspAnswerFailure.hpp"
#include "LspRename.hpp"
#include "LspWorkspaceEditFailure.hpp"
#include "LspWorkspaceEdits.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "PlaceCaret.hpp"
#include "QueryLanguage.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "Session.hpp"
#include "SettleRecentTab.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimKey.hpp"
#include "VimLanguageQuery.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
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
constexpr std::string_view lib_path = "C:\\work\\lib.cpp";
// ヘッダーは cpp の席が受ける .hpp（.h は C の言語で、この設定では C の席が無い）。
constexpr std::string_view header_path = "C:\\work\\lib.hpp";
constexpr std::string_view main_uri = "file:///C:/work/main.cpp";
constexpr std::string_view lib_uri = "file:///C:/work/lib.cpp";
constexpr std::string_view header_uri = "file:///C:/work/lib.hpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view main_text = "int add(int a, int b);\nint main() { return add(1, 2); }\n";
constexpr std::string_view lib_text = "int add(int a, int b) { return a + b; }\n";
constexpr std::string_view header_text = "int add(int a, int b);\n";
constexpr std::string_view renamed_main =
    "int sum(int a, int b);\nint main() { return sum(1, 2); }\n";
constexpr std::string_view half_renamed_main =
    "int sum(int a, int b);\nint main() { return add(1, 2); }\n";
constexpr std::string_view renamed_lib = "int sum(int a, int b) { return a + b; }\n";
constexpr std::string_view renamed_header = "int sum(int a, int b);\n";
constexpr std::string_view prepared =
    R"({"textDocumentSync":2,"renameProvider":{"prepareProvider":true}})";
constexpr std::string_view unprepared = R"({"textDocumentSync":2,"renameProvider":true})";
constexpr std::string_view prepare_method = "textDocument/prepareRename";
constexpr std::string_view rename_method = "textDocument/rename";
constexpr std::string_view add_target =
    R"({"range":{"start":{"line":1,"character":20},"end":{"line":1,"character":23}},"placeholder":"add"})";

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

void initialized(Editing &editing, std::string_view capabilities)
{
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                           std::string{capabilities} + "}}"}));
}

void caret_on_add(Editing &editing)
{
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{2}, core::Column{22}},
                            core::SelectionAnchoring::collapse});
}

// 3 つのファイルを置き、lib.hpp を開いてから main.cpp を開き、capabilities で `initialize`
// に答える。 lib.cpp はタブの無いファイル。キャレットは main.cpp の 2 行目の `add` の `d`。
void started(Editing &editing, std::string_view capabilities)
{
    editing.files().hold_at(std::string(main_path), Bytes{std::string(main_text)});
    editing.files().hold_at(std::string(lib_path), Bytes{std::string(lib_text)});
    editing.files().hold_at(std::string(header_path), Bytes{std::string(header_text)});
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    applied(editing.controller(), app::OpenDocument{path_of(header_path)});
    applied(editing.controller(), app::OpenDocument{path_of(main_path)});
    initialized(editing, capabilities);
    caret_on_add(editing);
}

app::EditorFrame answered(Editing &editing, std::string_view method, std::string_view result)
{
    const auto ids = editing.language_server().requests(method);
    editing.language_server().answer(ids.empty() ? -1 : ids.back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] std::string notice(Editing &editing, core::UiMessage message)
{
    return std::string(core::ui_text(message, editing.controller().display_language()));
}

[[nodiscard]] std::string filled(Editing &editing, core::UiMessage message,
                                 std::initializer_list<std::string_view> values)
{
    return core::ui_text_filled(message, editing.controller().display_language(), values);
}

// 本文の上の 1 区間の `TextEdit`（行と桁は 0 始まり）。
[[nodiscard]] std::string edit_json(std::size_t line, std::size_t start, std::size_t end,
                                    std::string_view text)
{
    const std::string at = R"({"line":)" + std::to_string(line) + R"(,"character":)";
    return R"({"newText":")" + std::string{text} + R"(","range":{"start":)" + at +
           std::to_string(start) + R"(},"end":)" + at + std::to_string(end) + "}}}";
}

[[nodiscard]] std::string add_to_sum()
{
    return edit_json(0, 4, 7, "sum");
}

// `changes` の 1 ファイル（"uri":[edits]）。
[[nodiscard]] std::string uri_edits(std::string_view uri, const std::string &edits)
{
    return "\"" + std::string{uri} + R"(":[)" + edits + "]";
}

// clangd の名前の変更の形（`changes`・版なし・3 ファイル・下調べ §2）。
[[nodiscard]] std::string clangd_changes()
{
    return R"({"changes":{)" +
           uri_edits(main_uri, add_to_sum() + "," + edit_json(1, 20, 23, "sum")) + "," +
           uri_edits(lib_uri, add_to_sum()) + "," + uri_edits(header_uri, add_to_sum()) + "}}";
}

// `TextDocumentEdit` 1 つ（version は JSON の値の文字列・空なら欄を書かない）。
[[nodiscard]] std::string document_edit(std::string_view uri, std::string_view version,
                                        const std::string &edits)
{
    std::string identifier = R"({"uri":")" + std::string{uri} + "\"";
    if (!version.empty())
    {
        identifier += R"(,"version":)" + std::string{version};
    }
    return R"({"textDocument":)" + identifier + R"(},"edits":[)" + edits + "]}";
}

[[nodiscard]] std::expected<core::LspWorkspaceEdit, core::LspWorkspaceEditFailure>
read(std::string_view json)
{
    const auto parsed = core::parse_json(json);
    if (!parsed.has_value())
    {
        return std::unexpected{core::LspWorkspaceEditFailure::malformed};
    }
    return core::lsp_workspace_edit(parsed.value());
}

[[nodiscard]] std::optional<core::LspWorkspaceEditFailure> failure_of(std::string_view json)
{
    const auto edit = read(json);
    return edit.has_value() ? std::nullopt : std::optional{edit.error()};
}

// 入力行の今の文字を消して name を打ち、Enter。
app::EditorFrame typed_name(Editing &editing, std::string_view name)
{
    const auto line = editing.controller().frame().command_line;
    const std::size_t typed = line.has_value() ? line.value().text.size() : 0;
    for (std::size_t index = 0; index < typed; ++index)
    {
        static_cast<void>(
            editing.controller().apply(app::EditCommand{core::CommandEdit::backspace}));
    }
    static_cast<void>(editing.controller().apply(app::CommandText{std::string{name}}));
    return editing.controller().apply_frame(app::SubmitCommand{});
}

app::EditorFrame asked(Editing &editing)
{
    return editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::rename});
}

// F2 → 変えられる所の答え（placeholder は add）→ sum と打って Enter。
void renamed_to_sum(Editing &editing)
{
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, prepare_method, add_target));
    static_cast<void>(typed_name(editing, "sum"));
}

[[nodiscard]] std::string body_of_tab(Editing &editing, std::size_t position)
{
    applied(editing.controller(), app::SwitchTab{position});
    return whole_vim_body(editing.controller());
}

[[nodiscard]] bool tab_is(const app::EditorFrame &frame, std::size_t position,
                          std::string_view path, core::SaveState state)
{
    if (position >= frame.tabs.size())
    {
        return false;
    }
    const auto &tab = frame.tabs.at(position);
    const auto &shown = tab.path;
    return shown.has_value() && shown.value().text() == path && tab.save_state == state;
}

// ---------------------------------------------------------------- core の読み

void verify_reading_forms()
{
    const auto clangd = read(clangd_changes());
    expect(clangd.has_value() && clangd.value().files.size() == 3 &&
               clangd.value().files.at(0).uri == main_uri &&
               clangd.value().files.at(0).edits.size() == 2 &&
               !clangd.value().files.at(0).version.has_value() &&
               clangd.value().files.at(2).uri == header_uri,
           "the changes form keeps the files in order with no versions (clangd)");
    const auto gopls = read(R"({"documentChanges":[)" + document_edit(main_uri, "7", add_to_sum()) +
                            "," + document_edit(lib_uri, "0", add_to_sum()) + "]}");
    expect(gopls.has_value() &&
               gopls.value().files.at(0).version == std::optional<std::uint64_t>{7} &&
               gopls.value().files.at(1).version == std::optional<std::uint64_t>{0},
           "documentChanges keeps the versions, the 0 of an unopened file included (gopls)");
    const auto pyright =
        read(R"({"documentChanges":[)" + document_edit(main_uri, "null", add_to_sum()) + "]}");
    expect(pyright.has_value() && !pyright.value().files.at(0).version.has_value(),
           "a null version reads as no version (pyright)");
    const auto both =
        read(R"({"changes":{)" + uri_edits(lib_uri, add_to_sum()) + R"(},"documentChanges":[)" +
             document_edit(main_uri, "", add_to_sum()) + "]}");
    expect(both.has_value() && both.value().files.size() == 1 &&
               both.value().files.at(0).uri == main_uri,
           "documentChanges wins over changes");
    // 同じ URI の 2 つ目の項目は 1 つ目を当てた後の本文を指すので、つながずに全体を断る（L2）。
    expect(failure_of(R"({"documentChanges":[)" + document_edit(main_uri, "", add_to_sum()) + "," +
                      document_edit(main_uri, "", edit_json(1, 20, 23, "sum")) + "]}") ==
               core::LspWorkspaceEditFailure::malformed,
           "two items for one URI refuse the whole answer");
    const auto empty = read("null");
    const auto bare = read("{}");
    const auto no_edits = read(R"({"changes":{)" + uri_edits(main_uri, "") + "}}");
    expect(empty.has_value() && empty.value().files.empty() && bare.has_value() &&
               bare.value().files.empty() && no_edits.has_value() && no_edits.value().files.empty(),
           "null, an empty object and a file without edits change nothing");
}

[[nodiscard]] std::string annotated_answer(std::string_view confirm)
{
    const std::string annotated =
        R"({"newText":"sum","annotationId":"a","range":{"start":{"line":0,"character":4},"end":{"line":0,"character":7}}})";
    return R"({"changeAnnotations":{"a":{"label":"rename","needsConfirmation":)" +
           std::string{confirm} + R"(}},"documentChanges":[)" +
           document_edit(main_uri, "", annotated) + "]}";
}

// count 個のファイルに 1 つずつ編集する `changes`。
[[nodiscard]] std::string many_files(std::size_t count)
{
    std::string json = R"({"changes":{)";
    for (std::size_t index = 0; index < count; ++index)
    {
        json += (index == 0 ? "" : ",") +
                uri_edits("file:///C:/work/f" + std::to_string(index) + ".cpp", add_to_sum());
    }
    return json + "}}";
}

void verify_reading_refusals()
{
    using Failure = core::LspWorkspaceEditFailure;
    expect(
        failure_of(R"({"documentChanges":[{"kind":"create","uri":"file:///C:/work/new.cpp"}]})") ==
            Failure::resource_operation,
        "a resource operation refuses the whole answer");
    expect(failure_of(annotated_answer("true")) == Failure::needs_confirmation &&
               !failure_of(annotated_answer("false")).has_value(),
           "an edit pointing at an annotation that needs confirmation refuses; others read");
    expect(failure_of("[]") == Failure::malformed &&
               failure_of(R"({"changes":[]})") == Failure::malformed &&
               failure_of(R"({"changes":{"x":{}}})") == Failure::malformed &&
               failure_of(R"({"changes":{"x":[{"newText":"a"}]}})") == Failure::malformed &&
               failure_of(R"({"documentChanges":[)" +
                          document_edit(main_uri, "\"7\"", add_to_sum()) + "]}") ==
                   Failure::malformed &&
               failure_of(R"({"documentChanges":[{"edits":[]}]})") == Failure::malformed,
           "another shape, an edit without a range, a text version and a missing document fail");
    expect(!failure_of(many_files(core::lsp_workspace_edit_file_limit)).has_value() &&
               failure_of(many_files(core::lsp_workspace_edit_file_limit + 1)) ==
                   Failure::too_many_files,
           "32 files read and 33 files refuse");
}

[[nodiscard]] std::expected<std::optional<core::LspRenameTarget>, core::LspAnswerFailure>
target_of(std::string_view json)
{
    return core::lsp_rename_target(core::parse_json(json).value());
}

// 読んだ形の名前（invalid・refused・default・range・range+<placeholder>）。
[[nodiscard]] std::string target_shape(std::string_view json)
{
    const auto read = target_of(json);
    if (!read.has_value())
    {
        return "invalid";
    }
    const auto &target = read.value();
    if (!target.has_value())
    {
        return "refused";
    }
    const auto &range = target.value().range;
    const auto &placeholder = target.value().placeholder;
    if (!range.has_value())
    {
        return placeholder.has_value() ? "broken" : "default";
    }
    return placeholder.has_value() ? "range+" + placeholder.value() : "range";
}

void verify_rename_target_reading()
{
    expect(target_shape(R"({"start":{"line":1,"character":20},"end":{"line":1,"character":23}})") ==
               "range",
           "a Range reads as the range only");
    expect(target_shape(add_target) == "range+add", "a range with a placeholder keeps both");
    expect(target_shape(R"({"defaultBehavior":true})") == "default",
           "defaultBehavior true reads as the word under the caret");
    expect(target_shape("null") == "refused" &&
               target_shape(R"({"defaultBehavior":false})") == "refused",
           "null and defaultBehavior false cannot be renamed");
    expect(target_shape("42") == "invalid" && target_shape(R"({"range":{}})") == "invalid" &&
               target_shape(R"({"defaultBehavior":1})") == "invalid",
           "other shapes are invalid");
}

void verify_word_around()
{
    using Range = std::pair<std::size_t, std::size_t>;
    expect(core::completion_word_around("int add(1)", 5) == Range{4, 7} &&
               core::completion_word_around("int add(1)", 4) == Range{4, 7} &&
               core::completion_word_around("int add(1)", 7) == Range{4, 7} &&
               core::completion_word_around("int add(1)", 8) == Range{8, 9} &&
               core::completion_word_around("a + b", 2) == Range{2, 2},
           "the word around the caret follows the completion word rule");
}

// ---------------------------------------------------------------- 鍵と能力

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

void verify_keys()
{
    const core::KeyChord f2{false, false, core::OperationKey::f2};
    expect(
        core::operation_for(f2, core::EditMode::ordinary) == core::EditorOperation::rename_symbol &&
            core::operation_for(f2, core::EditMode::vim) == core::EditorOperation::rename_symbol &&
            core::key_chord_label(f2) == "F2",
        "F2 renames in both modes");
    expect(vim_queried(U"grn") == core::LanguageQueryKind::rename &&
               vim_queried(U"grr") == core::LanguageQueryKind::references &&
               vim_stepped(U"grn").failure == core::VimRepeatFailure::refused &&
               !vim_queried(U"dgrn").has_value() && !vim_queried(U"2grn").has_value(),
           "grn asks for the rename on the refused step of a key without a row like grr");
}

void verify_capabilities()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    const std::string &initialize = editing.language_server().sent().front();
    expect(
        contains(initialize, R"("rename":{"prepareSupport":true})") &&
            contains(
                initialize,
                R"("workspace":{"workspaceEdit":{"documentChanges":true,"failureHandling":"abort"}})"),
        "initialize offers rename with prepare support and document changes with abort");
    const auto &abilities = editing.controller().language_server_abilities();
    expect(abilities.rename && abilities.prepare_rename, "renameProvider with prepareProvider");
    Editing plain(lsp_on());
    started(plain, unprepared);
    expect(plain.controller().language_server_abilities().rename &&
               !plain.controller().language_server_abilities().prepare_rename,
           "renameProvider true has no prepare");
    Editing none(lsp_on());
    started(none, R"({"textDocumentSync":2})");
    const auto refused = asked(none);
    expect(message_of(refused) == notice(none, core::UiMessage::notice_rename_not_supported) &&
               none.language_server().requests(prepare_method).empty() &&
               !refused.command_line.has_value(),
           "a server without renameProvider is not asked and says so");
    Editing off(SettingsReading{std::nullopt});
    off.files().hold_at(std::string(main_path), Bytes{std::string(main_text)});
    applied(off.controller(), app::OpenDocument{path_of(main_path)});
    const auto stopped = asked(off);
    expect(message_of(stopped) ==
                   notice(off, core::UiMessage::notice_language_server_not_running) &&
               !stopped.command_line.has_value() && off.language_server().touches() == 0,
           "with LSP off rename says the server is not running and touches no server");
}

// ---------------------------------------------------------------- 入力行

void verify_input_line()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    const auto requested = asked(editing);
    expect(editing.language_server().requests(prepare_method).size() == 1 &&
               !requested.command_line.has_value() &&
               contains(editing.language_server().sent().back(),
                        R"("position":{"line":1,"character":21})"),
           "F2 asks where to rename at the caret before opening the line");
    const auto opened = answered(editing, prepare_method, add_target);
    expect(opened.command_line.has_value() &&
               opened.command_line.value().prompt == core::InputLinePrompt::rename &&
               opened.command_line.value().text == "add",
           "the placeholder is the first text of the new name line");
    static_cast<void>(editing.controller().apply_frame(app::SubmitCommand{}));
    expect(editing.language_server().requests(rename_method).empty(),
           "Enter with the same name asks nothing");
    static_cast<void>(asked(editing));
    const auto ranged =
        answered(editing, prepare_method,
                 R"({"start":{"line":1,"character":20},"end":{"line":1,"character":23}})");
    expect(ranged.command_line.has_value() && ranged.command_line.value().text == "add",
           "a range answer opens with the text of the range");
    const auto emptied = typed_name(editing, "");
    expect(editing.language_server().requests(rename_method).empty() &&
               !emptied.command_line.has_value(),
           "an empty name closes the line and asks nothing");
    static_cast<void>(asked(editing));
    const auto behaved = answered(editing, prepare_method, R"({"defaultBehavior":true})");
    expect(behaved.command_line.has_value() && behaved.command_line.value().text == "add",
           "defaultBehavior opens with the word under the caret");
    const auto cancelled = editing.controller().apply_frame(app::CancelCommand{});
    expect(!cancelled.command_line.has_value() &&
               editing.language_server().requests(rename_method).empty() &&
               whole_vim_body(editing.controller()) == main_text,
           "Esc closes the line without asking");
    // 頼んだ後にキャレットが動いたら、変えられる所の答えは使わない（文脈の照合・決定 1）。
    static_cast<void>(asked(editing));
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{1}},
                            core::SelectionAnchoring::collapse});
    const auto moved = answered(editing, prepare_method, add_target);
    expect(!moved.command_line.has_value() &&
               editing.language_server().requests(rename_method).empty(),
           "an answer after the caret moved opens no line");
}

void verify_rename_request_and_refusals()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    const auto renames = editing.language_server().requests(rename_method);
    expect(renames.size() == 1 &&
               contains(editing.language_server().sent().back(), R"("newName":"sum")") &&
               contains(editing.language_server().sent().back(),
                        R"("position":{"line":1,"character":21})"),
           "Enter asks for the rename with newName at the caret of the question");
    static_cast<void>(asked(editing));
    const auto refused = answered(editing, prepare_method, "null");
    expect(message_of(refused) == notice(editing, core::UiMessage::notice_rename_refused) &&
               !refused.command_line.has_value(),
           "a null answer cannot be renamed");
    static_cast<void>(asked(editing));
    const auto ids = editing.language_server().requests(prepare_method);
    const auto errored = delivered(
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" + std::to_string(ids.back()) +
                                    R"(,"error":{"code":-32801,"message":"content modified"}})"});
    expect(message_of(errored) == "content modified" && !errored.command_line.has_value(),
           "an error answer shows the server's message as it is");
    Editing plain(lsp_on());
    started(plain, unprepared);
    const auto opened = asked(plain);
    expect(opened.command_line.has_value() && opened.command_line.value().text == "add" &&
               plain.language_server().requests(prepare_method).empty(),
           "without prepareProvider the line opens at once with the word under the caret");
}

// ---------------------------------------------------------------- 複数のファイルに当てる

void verify_rename_across_files()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    const auto done = answered(editing, rename_method, clangd_changes());
    expect(message_of(done) ==
               filled(editing, core::UiMessage::notice_server_edit_applied_files, {"3", "4"}),
           "the answer says how many places in how many files changed");
    // 帯は lib.hpp・main.cpp の順で、開いていなかった lib.cpp
    // は当てたときのアクティブ（main.cpp）の右。
    expect(done.active_tab == 1 && done.tabs.size() == 3 &&
               tab_is(done, 0, header_path, core::SaveState::modified) &&
               tab_is(done, 1, main_path, core::SaveState::modified) &&
               tab_is(done, 2, lib_path, core::SaveState::modified),
           "the asking tab stays active and the unopened file opens as a modified tab");
    expect(whole_vim_body(editing.controller()) == renamed_main &&
               done.caret.position == core::TextPosition{core::LineNumber{2}, core::Column{24}} &&
               editing.files().written_path().empty(),
           "the asking document is renamed, the caret keeps its name and nothing is saved");
    expect(body_of_tab(editing, 2) == renamed_lib && body_of_tab(editing, 0) == renamed_header,
           "the other files are renamed too");
}

void verify_undo_and_recency()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    static_cast<void>(answered(editing, rename_method, clangd_changes()));
    const auto walked = editing.controller().apply_frame(app::WalkRecentTab{core::TabStep::next});
    expect(walked.active_tab == 2,
           "the newly opened file is right after the asking tab in Ctrl+Tab");
    static_cast<void>(editing.controller().apply(app::SettleRecentTab{}));
    applied(editing.controller(), app::SwitchTab{1});
    const auto undone =
        editing.controller().apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    // 通常モードの元に戻すのキャレットは Windows の標準（戻した区間の終わり・D82）。戻り先の
    // キャレット（Edit.restore）は Vim の `u` が読む（verify_vim_grn）。
    expect(whole_vim_body(editing.controller()) == main_text &&
               undone.caret.position == core::TextPosition{core::LineNumber{2}, core::Column{24}},
           "one undo in the asking document brings its text back");
    applied(editing.controller(), app::SwitchTab{2});
    static_cast<void>(
        editing.controller().apply_frame(app::HistoryAction{core::HistoryDirection::undo}));
    expect(whole_vim_body(editing.controller()) == lib_text &&
               body_of_tab(editing, 0) == renamed_header,
           "each document undoes its own unit and the others stay renamed");
}

void verify_versions()
{
    Editing gopls(lsp_on());
    started(gopls, prepared);
    renamed_to_sum(gopls);
    const auto applied_versions = answered(
        gopls, rename_method,
        R"({"documentChanges":[)" +
            document_edit(main_uri, "0", add_to_sum() + "," + edit_json(1, 20, 23, "sum")) + "," +
            document_edit(lib_uri, "0", add_to_sum()) + "]}");
    expect(whole_vim_body(gopls.controller()) == renamed_main && applied_versions.tabs.size() == 3,
           "the version of an opened document matches and the 0 of an unopened file is not read");
    Editing stale(lsp_on());
    started(stale, prepared);
    renamed_to_sum(stale);
    const auto refused =
        answered(stale, rename_method,
                 R"({"documentChanges":[)" + document_edit(main_uri, "5", add_to_sum()) + "]}");
    expect(message_of(refused) == notice(stale, core::UiMessage::notice_server_edit_stale) &&
               whole_vim_body(stale.controller()) == main_text,
           "an answered version other than the revision changes nothing");
    Editing typed(lsp_on());
    started(typed, prepared);
    renamed_to_sum(typed);
    applied(typed.controller(), app::InsertText{"x"});
    const auto moved = answered(typed, rename_method, clangd_changes());
    expect(message_of(moved) == notice(typed, core::UiMessage::notice_server_edit_stale) &&
               moved.tabs.size() == 2 && body_of_tab(typed, 0) == header_text,
           "a document typed into after asking changes nothing anywhere");
}

// 断る答え・その 1 行の文・文に入れるファイルの名前（無ければ空）。どれも 1 つも変えない（タブも
// 増えない）。
using RefusedAnswer = std::tuple<std::string, core::UiMessage, std::string_view>;

[[nodiscard]] std::vector<RefusedAnswer> refused_answers()
{
    const std::string main_and_gone = uri_edits(main_uri, add_to_sum()) + "," +
                                      uri_edits("file:///C:/work/gone.cpp", add_to_sum());
    using Message = core::UiMessage;
    return {
        {R"({"changes":{)" + uri_edits(main_uri, add_to_sum() + "," + edit_json(0, 5, 6, "x")) +
             "}}",
         Message::notice_server_edit_unreadable,
         {}},
        {R"({"changes":{)" + uri_edits("untitled:Untitled-1", add_to_sum()) + "}}",
         Message::notice_server_edit_not_a_file,
         {}},
        {R"({"changes":{)" + main_and_gone + "}}", Message::notice_server_edit_unreachable,
         "gone.cpp"},
        {R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
             uri_edits("file:///C:/work/big.cpp", add_to_sum()) + "}}",
         Message::notice_server_edit_unreachable, "big.cpp"},
        {R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
             uri_edits("file:///C:/work/giant.cpp", add_to_sum()) + "}}",
         Message::notice_server_edit_giant_line, "giant.cpp"},
        {R"({"documentChanges":[)" + document_edit(main_uri, "", add_to_sum()) +
             R"(,{"kind":"delete","uri":"file:///C:/work/lib.cpp"}]})",
         Message::notice_server_edit_resource_operation,
         {}},
        {annotated_answer("true"), Message::notice_server_edit_needs_confirmation, {}},
        {many_files(core::lsp_workspace_edit_file_limit + 1),
         Message::notice_server_edit_too_many_files,
         {}},
    };
}

[[nodiscard]] std::string expected_line(Editing &editing, const RefusedAnswer &refused)
{
    const auto &[answer, message, name] = refused;
    if (message == core::UiMessage::notice_server_edit_too_many_files)
    {
        return filled(editing, message, {std::to_string(core::lsp_workspace_edit_file_limit)});
    }
    return name.empty() ? notice(editing, message) : filled(editing, message, {name});
}

void verify_refusals_change_nothing()
{
    for (const RefusedAnswer &refused : refused_answers())
    {
        Editing editing(lsp_on());
        editing.files().hold_at("C:\\work\\big.cpp",
                                Bytes{std::string(std::size_t{4} * 1024 * 1024 + 1, 'a')});
        editing.files().hold_at("C:\\work\\giant.cpp", Bytes{std::string(20000, 'a') + "\n"});
        started(editing, prepared);
        renamed_to_sum(editing);
        const auto frame = answered(editing, rename_method, std::get<0>(refused));
        expect(message_of(frame) == expected_line(editing, refused) && frame.tabs.size() == 2 &&
                   frame.active_tab == 1 && whole_vim_body(editing.controller()) == main_text &&
                   frame.document.save_state == core::SaveState::saved,
               "a refused answer says why and changes nothing");
    }
}

void verify_busy_and_deferred()
{
    Editing busy(lsp_on());
    started(busy, prepared);
    renamed_to_sum(busy);
    static_cast<void>(busy.controller().apply(app::OpenCommandPalette{}));
    const auto arrived = answered(busy, rename_method, clangd_changes());
    const auto closed = busy.controller().apply_frame(app::CancelCommand{});
    expect(message_of(arrived) == notice(busy, core::UiMessage::notice_server_edit_busy) &&
               whole_vim_body(busy.controller()) == main_text && closed.tabs.size() == 2,
           "an answer that arrives while the Ctrl+P list is open says so and changes nothing");
    Editing deferred(lsp_on());
    started(deferred, prepared);
    deferred.reads().defer_above(8);
    renamed_to_sum(deferred);
    const auto frame = answered(deferred, rename_method, clangd_changes());
    expect(message_of(frame) ==
                   filled(deferred, core::UiMessage::notice_server_edit_unreachable, {"lib.cpp"}) &&
               !deferred.reads().cancels().empty() && frame.tabs.size() == 2 &&
               whole_vim_body(deferred.controller()) == main_text,
           "a file the reading port moves to the background is cancelled and refused");
}

// 1 回目の名前の変更で開いた lib.cpp と書き換えた lib.hpp は、当てる前に頼んだ席へ `didOpen` され、
// 続けて 2 回目の名前の変更が通る（#2011 のレビュー M1 の (b)）。
[[nodiscard]] bool opened_to_server(Editing &editing, std::string_view uri)
{
    const auto opens = editing.language_server().notifications("textDocument/didOpen");
    return std::ranges::any_of(opens,
                               [uri](const std::string &body) { return contains(body, uri); });
}

void verify_rename_twice()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    static_cast<void>(answered(editing, rename_method, clangd_changes()));
    const auto changes = editing.language_server().notifications("textDocument/didChange");
    expect(opened_to_server(editing, lib_uri) && opened_to_server(editing, header_uri) &&
               std::ranges::any_of(changes,
                                   [](const std::string &body) { return contains(body, lib_uri); }),
           "the files the rename changed are opened to the server and then changed");
    static_cast<void>(asked(editing));
    static_cast<void>(answered(
        editing, prepare_method,
        R"({"range":{"start":{"line":1,"character":20},"end":{"line":1,"character":23}},"placeholder":"sum"})"));
    static_cast<void>(typed_name(editing, "total"));
    const std::string to_total = edit_json(0, 4, 7, "total");
    const auto again = answered(
        editing, rename_method,
        R"({"changes":{)" + uri_edits(main_uri, to_total + "," + edit_json(1, 20, 23, "total")) +
            "," + uri_edits(lib_uri, to_total) + "," + uri_edits(header_uri, to_total) + "}}");
    expect(message_of(again) ==
                   filled(editing, core::UiMessage::notice_server_edit_applied_files, {"3", "4"}) &&
               contains(body_of_tab(editing, 2), "int total(int a, int b) {") &&
               contains(body_of_tab(editing, 0), "int total(int a, int b);"),
           "a second rename right after the first applies to the files the first one opened");
}

// lib.hpp を一度アクティブにして頼んだ席に開かせてから、main.cpp で頼む。
void started_with_header_opened(Editing &editing)
{
    started(editing, prepared);
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::SwitchTab{1});
    caret_on_add(editing);
}

// 照合 (b)(c): 頼んだ席が開いている lib.hpp を頼んだ後に打つ・lib.hpp の版が違う答えは、1
// つも変えない （#2011 のレビュー R4）。
void verify_other_tab_moved()
{
    Editing typed(lsp_on());
    started_with_header_opened(typed);
    renamed_to_sum(typed);
    applied(typed.controller(), app::SwitchTab{0});
    applied(typed.controller(), app::InsertText{"x"});
    applied(typed.controller(), app::SwitchTab{1});
    const auto moved = answered(typed, rename_method, clangd_changes());
    expect(message_of(moved) == notice(typed, core::UiMessage::notice_server_edit_stale) &&
               whole_vim_body(typed.controller()) == main_text && moved.tabs.size() == 2,
           "a didOpen'd tab typed into after asking changes nothing anywhere");
    Editing versioned(lsp_on());
    started_with_header_opened(versioned);
    renamed_to_sum(versioned);
    const auto refused =
        answered(versioned, rename_method,
                 R"({"documentChanges":[)" + document_edit(main_uri, "0", add_to_sum()) + "," +
                     document_edit(header_uri, "9", add_to_sum()) + "]}");
    expect(message_of(refused) == notice(versioned, core::UiMessage::notice_server_edit_stale) &&
               whole_vim_body(versioned.controller()) == main_text,
           "an answered version of another opened tab other than its revision changes nothing");
}

constexpr std::string_view notes_path = "C:\\work\\notes.txt";
constexpr std::string_view notes_uri = "file:///C:/work/notes.txt";

// 頼んだ席が開いていない（言語の違う）タブの答えを当てた本文（当てられなければ空）と 1 行。
[[nodiscard]] std::pair<std::string, std::string> unsent_tab(bool modified)
{
    Editing editing(lsp_on());
    editing.files().hold_at(std::string(notes_path), Bytes{std::string("add\n")});
    applied(editing.controller(), app::OpenDocument{path_of(notes_path)});
    if (modified)
    {
        applied(editing.controller(), app::InsertText{"x"});
    }
    started(editing, prepared);
    renamed_to_sum(editing);
    const auto frame = answered(editing, rename_method,
                                R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                                    uri_edits(notes_uri, edit_json(0, 0, 3, "sum")) + "}}");
    const std::string line = message_of(frame);
    const bool expected =
        modified
            ? line == filled(editing, core::UiMessage::notice_server_edit_unsent, {"notes.txt"})
            : line ==
                  filled(editing, core::UiMessage::notice_server_edit_applied_files, {"2", "2"});
    return {expected ? whole_vim_body(editing.controller()) : std::string{"(wrong line)"}, line};
}

// 頼んだ席が開いていないタブは、未保存なら全体を断り、保存済みなら当てる（M1 の (a)）。
void verify_unsent_tabs()
{
    expect(unsent_tab(true).first == main_text,
           "an unsaved tab the server never opened refuses the whole answer");
    expect(unsent_tab(false).first == half_renamed_main,
           "a saved tab the server never opened is renamed from its disk text");
}

// タブの無いファイルは UTF-8 のときだけ当てる（#2011 のレビュー R2）。
void verify_not_utf8()
{
    Editing editing(lsp_on());
    editing.files().hold_at("C:\\work\\old.cpp", Bytes{std::string("\x93\xFA\n")});
    editing.code_pages().decode_to(std::string("日\n"));
    started(editing, prepared);
    renamed_to_sum(editing);
    const auto frame =
        answered(editing, rename_method,
                 R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                     uri_edits("file:///C:/work/old.cpp", edit_json(0, 0, 1, "x")) + "}}");
    expect(message_of(frame) ==
                   filled(editing, core::UiMessage::notice_server_edit_not_utf8, {"old.cpp"}) &&
               whole_vim_body(editing.controller()) == main_text && frame.tabs.size() == 2,
           "an unopened file that is not UTF-8 refuses the whole answer");
    // BOM つきの UTF-16 も UTF-8 でないので同じく断る（plain_utf8 は BOM なしの UTF-8 だけ）。
    for (const std::string_view bytes : {std::string_view{"\xFF\xFE"
                                                          "a\0\n\0",
                                                          6},
                                         std::string_view{"\xFE\xFF"
                                                          "\0a\0\n",
                                                          6}})
    {
        Editing wide(lsp_on());
        wide.files().hold_at("C:\\work\\wide.cpp", Bytes{std::string(bytes)});
        started(wide, prepared);
        renamed_to_sum(wide);
        const auto refused =
            answered(wide, rename_method,
                     R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                         uri_edits("file:///C:/work/wide.cpp", edit_json(0, 0, 1, "x")) + "}}");
        expect(message_of(refused) ==
                       filled(wide, core::UiMessage::notice_server_edit_not_utf8, {"wide.cpp"}) &&
                   whole_vim_body(wide.controller()) == main_text,
               "an unopened UTF-16 file refuses the whole answer too");
    }
    // 読めない答え（changes が表でない）は 1 行で断り、本文を変えない（決定 4）。
    Editing broken(lsp_on());
    started(broken, prepared);
    renamed_to_sum(broken);
    const auto unread = answered(broken, rename_method, R"({"changes":5})");
    expect(message_of(unread) == notice(broken, core::UiMessage::notice_server_edit_unreadable) &&
               whole_vim_body(broken.controller()) == main_text,
           "an unreadable rename answer says so and changes nothing");
    // 今の本文と同じになる答えは何も変えず、何も知らせない（ADR 0194
    // の「何も変わらなければ何も出さない」）。
    Editing same(lsp_on());
    started(same, prepared);
    renamed_to_sum(same);
    const auto kept =
        answered(same, rename_method,
                 R"({"changes":{)" + uri_edits(main_uri, edit_json(0, 4, 7, "add")) + "}}");
    expect(message_of(kept).empty() && whole_vim_body(same.controller()) == main_text &&
               kept.document.save_state == core::SaveState::saved,
           "an answer that leaves the text as it is changes and says nothing");
}

// Ctrl+Tab の歩きの途中に届いた答えは当てない（#2011 のレビュー R1）。
void verify_tab_walk()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    static_cast<void>(editing.controller().apply(app::WalkRecentTab{core::TabStep::next}));
    static_cast<void>(editing.controller().apply(app::WalkRecentTab{core::TabStep::next}));
    const auto frame = answered(editing, rename_method, clangd_changes());
    expect(message_of(frame) == notice(editing, core::UiMessage::notice_server_edit_busy) &&
               frame.active_tab == 1 && editing.controller().tab_walking() &&
               whole_vim_body(editing.controller()) == main_text,
           "an answer during a Ctrl+Tab walk changes nothing and leaves the walk alone");
}

// null・空の答えと誤りの答えの 1 行（#2011 のレビュー R5）。
void verify_answer_lines()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    expect(message_of(answered(editing, rename_method, "null")) ==
               notice(editing, core::UiMessage::notice_rename_not_found),
           "a null rename answer finds nothing to rename");
    renamed_to_sum(editing);
    expect(message_of(answered(editing, rename_method, "{}")) ==
               notice(editing, core::UiMessage::notice_rename_not_found),
           "an empty rename answer finds nothing to rename");
    renamed_to_sum(editing);
    const auto ids = editing.language_server().requests(rename_method);
    const auto errored = delivered(
        editing, app::ServerMessage{R"({"jsonrpc":"2.0","id":)" + std::to_string(ids.back()) +
                                    R"(,"error":{"code":-32803,"message":"rename failed"}})"});
    expect(message_of(errored) == "rename failed" &&
               whole_vim_body(editing.controller()) == main_text,
           "an error answer shows the server's message as it is");
}

// Vim の INSERT で押した F2 は頼む前に断る（整形と同じ・#2011 のレビュー R5）。
void verify_insert_refusal()
{
    Editing insert(lsp_on());
    started(insert, prepared);
    applied(insert.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(insert.controller(), "i");
    const auto refused = asked(insert);
    expect(message_of(refused) == notice(insert, core::UiMessage::notice_rename_in_insert) &&
               insert.language_server().requests(prepare_method).empty() &&
               !refused.command_line.has_value(),
           "F2 in Insert mode is refused before asking");
}

// 頼んだときは未保存だった、席が開いていないタブを、答えの前に保存しても当てない（保存では改訂番号が
// 動かないので、頼んだときの保存の状態で照らす・#2011 のレビュー R6 の N1）。
void verify_saved_after_asking()
{
    Editing editing(lsp_on());
    editing.files().hold_at(std::string(notes_path), Bytes{std::string("add\n")});
    applied(editing.controller(), app::OpenDocument{path_of(notes_path)});
    applied(editing.controller(), app::InsertText{"x"});
    started(editing, prepared);
    renamed_to_sum(editing);
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::SaveDocument{path_of(notes_path), core::TextEncoding::utf8});
    applied(editing.controller(), app::SwitchTab{2});
    const auto frame = answered(editing, rename_method,
                                R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                                    uri_edits(notes_uri, edit_json(0, 0, 3, "sum")) + "}}");
    expect(message_of(frame) ==
                   filled(editing, core::UiMessage::notice_server_edit_unsent, {"notes.txt"}) &&
               whole_vim_body(editing.controller()) == main_text,
           "a tab unsaved when asking and saved before the answer is not renamed");
}

// 頼んだ後に開いて打ち、保存したタブは、写しに無く読んだ後に変わったので当てない（N1）。
void verify_opened_after_asking()
{
    Editing editing(lsp_on());
    editing.files().hold_at(std::string(notes_path), Bytes{std::string("add\n")});
    started(editing, prepared);
    renamed_to_sum(editing);
    applied(editing.controller(), app::OpenDocument{path_of(notes_path)});
    applied(editing.controller(), app::InsertText{"x"});
    applied(editing.controller(), app::SaveDocument{path_of(notes_path), core::TextEncoding::utf8});
    applied(editing.controller(), app::SwitchTab{1});
    const auto frame = answered(editing, rename_method,
                                R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                                    uri_edits(notes_uri, edit_json(0, 0, 3, "sum")) + "}}");
    expect(message_of(frame) == notice(editing, core::UiMessage::notice_server_edit_stale) &&
               whole_vim_body(editing.controller()) == main_text,
           "a tab opened, typed into and saved after asking is not renamed");
}

constexpr std::string_view japanese_path = "C:\\work\\old.txt";
constexpr std::string_view japanese_uri = "file:///C:/work/old.txt";

// 答えが main.cpp と old.txt（Shift_JIS）を指す。
[[nodiscard]] std::string with_japanese_file()
{
    return R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
           uri_edits(japanese_uri, edit_json(0, 0, 1, "x")) + "}}";
}

// 席が開いていない当て先は、読み込み済みのタブもまだ読んでいないタブも、BOM なしの UTF-8 のときだけ
// 当てる（N2）。BOM つきは clangd が 1 行目の BOM を 1 文字と数えた（s7a-rename の
// answers-bom.json）。
void verify_not_utf8_tabs()
{
    Editing opened(lsp_on());
    opened.files().hold_at(std::string(japanese_path), Bytes{std::string("\x93\xFA\n")});
    opened.code_pages().decode_to(std::string("日\n"));
    applied(opened.controller(), app::OpenDocument{path_of(japanese_path)});
    started(opened, prepared);
    renamed_to_sum(opened);
    expect(message_of(answered(opened, rename_method, with_japanese_file())) ==
                   filled(opened, core::UiMessage::notice_server_edit_not_utf8, {"old.txt"}) &&
               whole_vim_body(opened.controller()) == main_text,
           "a saved Shift_JIS tab the server never opened refuses the whole answer");
    const auto listed = [](std::string_view path, std::size_t rank)
    {
        return app::SessionTab{path_of(path),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    Editing unread(lsp_on(),
                   SessionReading{std::optional<app::Session>{
                       app::Session{{listed(main_path, 0), listed(japanese_path, 1)}, 0}}},
                   [](ScriptedFiles &files)
                   {
                       files.hold_at(std::string(main_path), Bytes{std::string(main_text)});
                       files.hold_at(std::string(japanese_path), Bytes{std::string("\x93\xFA\n")});
                   });
    unread.code_pages().decode_to(std::string("日\n"));
    static_cast<void>(unread.controller().apply(app::VisibleLines{20}));
    initialized(unread, prepared);
    caret_on_add(unread);
    renamed_to_sum(unread);
    expect(message_of(answered(unread, rename_method, with_japanese_file())) ==
                   filled(unread, core::UiMessage::notice_server_edit_not_utf8, {"old.txt"}) &&
               whole_vim_body(unread.controller()) == main_text,
           "a Shift_JIS tab not read yet is read and then refuses the whole answer");
    Editing bom(lsp_on());
    bom.files().hold_at("C:\\work\\bom.cpp", Bytes{std::string("\xEF\xBB\xBFint add;\n")});
    started(bom, prepared);
    renamed_to_sum(bom);
    const auto refused = answered(bom, rename_method,
                                  R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                                      uri_edits("file:///C:/work/bom.cpp", add_to_sum()) + "}}");
    expect(message_of(refused) ==
                   filled(bom, core::UiMessage::notice_server_edit_not_utf8, {"bom.cpp"}) &&
               refused.tabs.size() == 2,
           "an unopened UTF-8 file with a BOM refuses the whole answer");
}

// 送った本文の中の位置（無ければ sent の数）。
[[nodiscard]] std::size_t sent_index(Editing &editing, std::string_view method,
                                     std::string_view uri)
{
    const auto &sent = editing.language_server().sent();
    for (std::size_t index = 0; index < sent.size(); ++index)
    {
        if (contains(sent.at(index), method) && contains(sent.at(index), uri))
        {
            return index;
        }
    }
    return sent.size();
}

// (b) の順と版: 新しく開いた lib.cpp は、当てる前の本文と版 0 で `didOpen`、当てた後の版 1 の
// `didChange` の順（#2011 のレビュー R6 の P2）。
void verify_open_then_change_order()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    static_cast<void>(answered(editing, rename_method, clangd_changes()));
    const auto &sent = editing.language_server().sent();
    const std::size_t opened = sent_index(editing, "textDocument/didOpen", lib_uri);
    const std::size_t changed = sent_index(editing, "textDocument/didChange", lib_uri);
    expect(opened < changed && changed < sent.size() &&
               contains(sent.at(opened), R"("version":0)") &&
               contains(sent.at(opened), "int add(int a, int b) { return a + b; }") &&
               contains(sent.at(changed), R"("version":1)"),
           "the opened file goes out with its old text at version 0 before the change at 1");
}

// 綴りの違う URI が同じファイルを指す 2 項目は、つながずに全体を断る（P3）。
void verify_same_file_spellings()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    renamed_to_sum(editing);
    const auto frame =
        answered(editing, rename_method,
                 R"({"changes":{)" + uri_edits(main_uri, add_to_sum()) + "," +
                     uri_edits("file:///c:/work/main.cpp", edit_json(1, 20, 23, "sum")) + "}}");
    expect(message_of(frame) == notice(editing, core::UiMessage::notice_server_edit_unreadable) &&
               whole_vim_body(editing.controller()) == main_text,
           "two spellings of one file refuse the whole answer");
}

// 状態を動かさない検査で断れば、まだ読んでいないタブは読まない（R3・P3）。
void verify_unread_tab_kept()
{
    const auto listed = [](std::string_view path, std::size_t rank)
    {
        return app::SessionTab{path_of(path),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    Editing editing(lsp_on(),
                    SessionReading{std::optional<app::Session>{
                        app::Session{{listed(main_path, 0), listed(header_path, 1)}, 0}}},
                    [](ScriptedFiles &files)
                    {
                        files.hold_at(std::string(main_path), Bytes{std::string(main_text)});
                        files.hold_at(std::string(header_path), Bytes{std::string(header_text)});
                    });
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    initialized(editing, prepared);
    caret_on_add(editing);
    renamed_to_sum(editing);
    const auto frame = answered(editing, rename_method,
                                R"({"changes":{)" + uri_edits(header_uri, add_to_sum()) + "," +
                                    uri_edits("file:///C:/work/gone.cpp", add_to_sum()) + "}}");
    const auto &requests = editing.reads().requests();
    expect(message_of(frame) ==
                   filled(editing, core::UiMessage::notice_server_edit_unreachable, {"gone.cpp"}) &&
               std::ranges::none_of(requests, [](const app::DocumentReadRequest &request)
                                    { return request.path.text() == header_path; }),
           "a refusal before reaching tabs leaves the tab not read yet");
}

constexpr std::string_view c_header_path = "C:\\work\\lib.h";
constexpr std::string_view c_header_uri = "file:///C:/work/lib.h";

// cpp だけの設定で `.h`（C の言語）は席に開かれない。保存済みなら当たるが、当てた後は保存するまで
// 次の名前の変更がそのファイルで断られる（#2011 のレビュー R6 の P4・ADR の限界）。
// c と cpp に同じ命令を置くと、2 つの言語は同じ席を共有する（#1952・ADR 0161 の 2026-11-24 の
// 追補）。
[[nodiscard]] SettingsReading lsp_shared()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"c", std::string{clangd_command}},
                             {"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

// `.h`（C の言語）を指す名前の変更を 2 回。shared なら c と cpp が席を共有し、1 回目に当てた `.h`
// は 席に開かれて 2 回目も当たる。cpp だけの設定では `.h` は席に開かれず、2 回目は保存するまで断る
// （#2011 のレビュー R6 の P4・ADR の限界）。
void verify_c_header(bool shared)
{
    Editing editing(shared ? lsp_shared() : lsp_on());
    editing.files().hold_at(std::string(main_path), Bytes{std::string(main_text)});
    editing.files().hold_at(std::string(c_header_path), Bytes{std::string(header_text)});
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    applied(editing.controller(), app::OpenDocument{path_of(c_header_path)});
    applied(editing.controller(), app::OpenDocument{path_of(main_path)});
    initialized(editing, prepared);
    caret_on_add(editing);
    renamed_to_sum(editing);
    const auto first = answered(
        editing, rename_method,
        R"({"changes":{)" + uri_edits(main_uri, add_to_sum() + "," + edit_json(1, 20, 23, "sum")) +
            "," + uri_edits(c_header_uri, add_to_sum()) + "}}");
    const bool opened = sent_index(editing, "textDocument/didOpen", c_header_uri) <
                        editing.language_server().sent().size();
    expect(message_of(first) ==
                   filled(editing, core::UiMessage::notice_server_edit_applied_files, {"2", "3"}) &&
               opened == shared,
           shared ? "a shared seat opens the renamed C header"
                  : "a saved C header is renamed but not opened to the cpp-only seat");
    static_cast<void>(asked(editing));
    static_cast<void>(answered(
        editing, prepare_method,
        R"({"range":{"start":{"line":1,"character":20},"end":{"line":1,"character":23}},"placeholder":"sum"})"));
    static_cast<void>(typed_name(editing, "total"));
    const auto second =
        answered(editing, rename_method,
                 R"({"changes":{)" + uri_edits(c_header_uri, edit_json(0, 4, 7, "total")) + "}}");
    expect(message_of(second) ==
               (shared ? filled(editing, core::UiMessage::notice_server_edit_applied, {"1"})
                       : filled(editing, core::UiMessage::notice_server_edit_unsent, {"lib.h"})),
           shared ? "the next rename applies to the C header the shared seat opened"
                  : "the next rename refuses the modified C header until it is saved");
}

// まだ読んでいないタブ（前回のタブ）は reach_tab で読んでから当てる。
void verify_unread_tab()
{
    const auto listed = [](std::string_view path, std::size_t rank)
    {
        return app::SessionTab{path_of(path),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    Editing editing(lsp_on(),
                    SessionReading{std::optional<app::Session>{
                        app::Session{{listed(main_path, 0), listed(header_path, 1)}, 0}}},
                    [](ScriptedFiles &files)
                    {
                        files.hold_at(std::string(main_path), Bytes{std::string(main_text)});
                        files.hold_at(std::string(header_path), Bytes{std::string(header_text)});
                    });
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    initialized(editing, prepared);
    caret_on_add(editing);
    renamed_to_sum(editing);
    const auto done = answered(editing, rename_method,
                               R"({"changes":{)" + uri_edits(header_uri, add_to_sum()) + "}}");
    expect(message_of(done) ==
                   filled(editing, core::UiMessage::notice_server_edit_applied, {"1"}) &&
               done.active_tab == 0 && tab_is(done, 1, header_path, core::SaveState::modified) &&
               body_of_tab(editing, 1) == renamed_header,
           "a tab not read yet is read, renamed and left modified");
}

// 実物の clangd 19.1.5（Build Tools の LLVM）に S7 a と同じ client の能力で頼んだ答え（2026-11-24・
// D:/NeNeNib/scripts/20261124/s7a-rename の answers.json）。documentChanges を名乗っても `changes`
// で 3 ファイル（開いていない lib.cpp と lib.h を含む）を返した。
constexpr std::string_view probe_root = R"(D:\NeNeNib\scripts\20261124\s7a-rename\cpp\src\)";
constexpr std::string_view probe_main =
    "#include \"lib.h\"\nint main() {\n    int total = greet_count(2)\n    return total;\n}\n";
constexpr std::string_view probe_lib =
    "#include \"lib.h\"\nint greet_count(int base) { return base + 1; }\n";
constexpr std::string_view probe_header = "#pragma once\nint greet_count(int base);\n";
constexpr std::string_view clangd_rename_answer =
    R"({"changes":{"file:///D:/NeNeNib/scripts/20261124/s7a-rename/cpp/src/lib.cpp":[{"newText":"nib_renamed","range":{"end":{"character":15,"line":1},"start":{"character":4,"line":1}}}],)"
    R"("file:///D:/NeNeNib/scripts/20261124/s7a-rename/cpp/src/lib.h":[{"newText":"nib_renamed","range":{"end":{"character":15,"line":1},"start":{"character":4,"line":1}}}],)"
    R"("file:///D:/NeNeNib/scripts/20261124/s7a-rename/cpp/src/main.cpp":[{"newText":"nib_renamed","range":{"end":{"character":27,"line":2},"start":{"character":16,"line":2}}}]}})";

void verify_clangd_answer()
{
    const std::string root{probe_root};
    Editing editing(lsp_on());
    editing.files().hold_at(root + "main.cpp", Bytes{std::string(probe_main)});
    editing.files().hold_at(root + "lib.cpp", Bytes{std::string(probe_lib)});
    editing.files().hold_at(root + "lib.h", Bytes{std::string(probe_header)});
    static_cast<void>(editing.controller().apply(app::VisibleLines{20}));
    applied(editing.controller(), app::OpenDocument{path_of(root + "main.cpp")});
    initialized(editing, R"({"textDocumentSync":2,"renameProvider":{"prepareProvider":true}})");
    applied(editing.controller(),
            app::PlaceCaret{core::TextPosition{core::LineNumber{3}, core::Column{18}},
                            core::SelectionAnchoring::collapse});
    static_cast<void>(asked(editing));
    const auto opened = answered(
        editing, prepare_method,
        R"({"placeholder":"greet_count","range":{"end":{"character":27,"line":2},"start":{"character":16,"line":2}}})");
    expect(opened.command_line.has_value() && opened.command_line.value().text == "greet_count",
           "the clangd placeholder opens the new name line");
    static_cast<void>(typed_name(editing, "nib_renamed"));
    const auto done = answered(editing, rename_method, clangd_rename_answer);
    expect(message_of(done) ==
                   filled(editing, core::UiMessage::notice_server_edit_applied_files, {"3", "3"}) &&
               done.tabs.size() == 3 && done.active_tab == 0 &&
               contains(whole_vim_body(editing.controller()), "int total = nib_renamed(2)") &&
               contains(body_of_tab(editing, 1), "int nib_renamed(int base) {") &&
               contains(body_of_tab(editing, 2), "int nib_renamed(int base);"),
           "the real clangd answer renames the open file and opens the two others modified");
}

void verify_vim_grn()
{
    Editing editing(lsp_on());
    started(editing, prepared);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "grn");
    expect(editing.language_server().requests(prepare_method).size() == 1,
           "grn asks where to rename");
    const auto opened = answered(editing, prepare_method, add_target);
    expect(opened.command_line.has_value() &&
               opened.command_line.value().prompt == core::InputLinePrompt::rename,
           "the answer opens the new name line in vim");
    static_cast<void>(typed_name(editing, "sum"));
    static_cast<void>(answered(editing, rename_method, clangd_changes()));
    expect(whole_vim_body(editing.controller()) == renamed_main, "grn renames in vim");
    vim_replay(editing.controller(), "u");
    expect(whole_vim_body(editing.controller()) == main_text &&
               editing.controller().frame().caret.position ==
                   core::TextPosition{core::LineNumber{2}, core::Column{22}},
           "u brings the document and the caret before the rename back");
}
} // namespace

void verify_lsp_rename_scope()
{
    verify_reading_forms();
    verify_reading_refusals();
    verify_rename_target_reading();
    verify_word_around();
    verify_keys();
    verify_capabilities();
    verify_input_line();
    verify_rename_request_and_refusals();
    verify_rename_across_files();
    verify_undo_and_recency();
    verify_versions();
    verify_refusals_change_nothing();
    verify_busy_and_deferred();
    verify_unread_tab();
    verify_clangd_answer();
    verify_rename_twice();
    verify_other_tab_moved();
    verify_unsent_tabs();
    verify_not_utf8();
    verify_tab_walk();
    verify_answer_lines();
    verify_insert_refusal();
    verify_vim_grn();
    verify_saved_after_asking();
    verify_opened_after_asking();
    verify_not_utf8_tabs();
    verify_open_then_change_order();
    verify_same_file_spellings();
    verify_unread_tab_kept();
    verify_c_header(false);
    verify_c_header(true);
}
} // namespace nenenib::tests
