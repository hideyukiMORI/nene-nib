// scope `--diagnostic-jump` の契約（#1939・ADR 0161 の決定 9 の追補）。診断の間を跳ぶ鍵（Vim の
// `]d` `[d`・通常モードの F8 / Shift+F8）・札（`gl`・操作「診断を表示」）・Ctrl+P の一覧（`^`）。
// core の純関数と表、engine の効果、controller の道を替え玉 ScriptedLanguageServer で確かめる。
#include "ActivateCommandChoice.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "Diagnostic.hpp"
#include "DiagnosticCommand.hpp"
#include "DiagnosticJump.hpp"
#include "DiagnosticSeverity.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "KeyChord.hpp"
#include "LineNumber.hpp"
#include "LspDiagnostics.hpp"
#include "LspNotification.hpp"
#include "LspPositionEncoding.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"
#include "PlaceCaret.hpp"
#include "RunDiagnosticCommand.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimAction.hpp"
#include "VimCharacter.hpp"
#include "VimDiagnosticCommand.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimKeyTable.hpp"
#include "VimNoEffect.hpp"
#include "VimRepeatFailure.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::Diagnostic;
using core::DiagnosticSeverity;
using core::Offset;
using core::OffsetRange;

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view source_uri = "file:///C:/work/main.cpp";
constexpr std::string_view second_path = "C:\\work\\two.cpp";
constexpr std::string_view second_uri = "file:///C:/work/two.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
// 2 行の本文（1 行目 `int main() {}`・2 行目 `int x;`）。
constexpr std::string_view source_text = "int main() {}\nint x;\n";
// main.cpp の 3 つの診断: 1 行目の `main` の error（文は 2 行・source と code つき）、同じ所の
// hint、2 行目の `x` の warning。
constexpr std::string_view three_diagnostics =
    R"([{"range":{"start":{"line":1,"character":4},"end":{"line":1,"character":5}},"severity":2,"message":"unused x"},)"
    R"({"range":{"start":{"line":0,"character":4},"end":{"line":0,"character":8}},"severity":1,"message":"bad main\nsecond line","source":"clangd","code":42},)"
    R"({"range":{"start":{"line":0,"character":4},"end":{"line":0,"character":8}},"severity":4,"message":"a hint"}])";

[[nodiscard]] Diagnostic diagnostic_at(std::size_t begin, std::size_t end,
                                       DiagnosticSeverity severity, std::string message)
{
    return Diagnostic{OffsetRange{Offset{begin}, Offset{end}}, severity, std::move(message)};
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

[[nodiscard]] bool caret_on(const app::EditorFrame &frame, std::size_t line, std::size_t column)
{
    return frame.caret.position == core::TextPosition{core::LineNumber{line}, core::Column{column}};
}

[[nodiscard]] std::string tip_of(Editing &editing)
{
    const auto &hover = editing.controller().language_answers().hover;
    return hover.has_value() ? hover.value().text.value_or("<empty>") : "<none>";
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

// main.cpp（と two.cpp）を置き、main.cpp を開いて `initialize` に答え、three_diagnostics を
// 届かせる。
void diagnosed(Editing &editing)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    editing.files().hold_at(std::string(second_path), Bytes{std::string("int y;\n")});
    static_cast<void>(editing.controller().apply(app::VisibleLines{4}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{
            R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"textDocumentSync":2}}})"}));
    editing.language_server().publish(source_uri, 0, three_diagnostics);
    static_cast<void>(editing.controller().apply(app::WorkCompleted{}));
}

app::EditorFrame commanded(Editing &editing, core::DiagnosticCommand command)
{
    return editing.controller().apply_frame(app::RunDiagnosticCommand{command});
}

// ---------------------------------------------------------------- core の純関数と表

void verify_landing()
{
    const std::vector<Diagnostic> diagnostics{
        diagnostic_at(10, 12, DiagnosticSeverity::warning, "c"),
        diagnostic_at(2, 4, DiagnosticSeverity::error, "a"),
        diagnostic_at(2, 3, DiagnosticSeverity::hint, "a2"),
        diagnostic_at(6, 6, DiagnosticSeverity::information, "b")};
    const auto next = [&](std::size_t caret, std::size_t count)
    { return core::diagnostic_landing(diagnostics, Offset{caret}, true, count); };
    const auto previous = [&](std::size_t caret, std::size_t count)
    { return core::diagnostic_landing(diagnostics, Offset{caret}, false, count); };
    expect(next(0, 1) == Offset{2} && next(2, 1) == Offset{6} && next(3, 1) == Offset{6} &&
               next(6, 1) == Offset{10},
           "]d goes to the nearest start after the caret, counting one position once");
    expect(next(10, 1) == Offset{2} && next(11, 1) == Offset{2},
           "]d after the last diagnostic wraps to the first");
    expect(next(0, 2) == Offset{6} && next(0, 3) == Offset{10} && next(0, 4) == Offset{2} &&
               next(0, 300'000'002) == Offset{6},
           "a count walks that many positions around the list");
    expect(previous(11, 1) == Offset{10} && previous(10, 1) == Offset{6} &&
               previous(3, 1) == Offset{2} && previous(2, 1) == Offset{10} &&
               previous(0, 2) == Offset{6},
           "[d goes to the nearest start before the caret and wraps to the last");
    expect(!core::diagnostic_landing({}, Offset{0}, true, 1).has_value(),
           "without diagnostics there is nowhere to go");
    expect(core::diagnostic_order(diagnostics) == std::vector<std::size_t>{1, 2, 3, 0},
           "the list order is line, column, then the heavier first");
}

void verify_tip_text()
{
    std::vector<Diagnostic> diagnostics{diagnostic_at(0, 4, DiagnosticSeverity::hint, "h"),
                                        diagnostic_at(1, 3, DiagnosticSeverity::error, "e1\ne2"),
                                        diagnostic_at(8, 8, DiagnosticSeverity::warning, "w")};
    diagnostics.at(1).source = "clangd";
    diagnostics.at(1).code = "unused";
    diagnostics.at(0).code = "7";
    expect(core::diagnostic_tip_text(diagnostics, Offset{2}) == "e1\ne2\nclangd(unused)\n\nh\n7",
           "the tip shows every covering diagnostic heaviest first with its source and code");
    expect(core::diagnostic_tip_text(diagnostics, Offset{8}) == "w" &&
               core::diagnostic_tip_text(diagnostics, Offset{5}).empty(),
           "an empty range covers only its start; elsewhere there is no tip");
    expect(core::diagnostic_severity_mark(DiagnosticSeverity::error) == "E" &&
               core::diagnostic_severity_mark(DiagnosticSeverity::warning) == "W" &&
               core::diagnostic_severity_mark(DiagnosticSeverity::information) == "I" &&
               core::diagnostic_severity_mark(DiagnosticSeverity::hint) == "H",
           "the list marks match the status bar's E and W");
}

// 文の全部・source・code（整数は 10 進）を読み、長い文は上限のバイトで code point を割らずに切る。
void verify_parsed_fields()
{
    const auto text = core::TextBuffer::from_utf8("ab").value();
    std::string long_message(core::lsp_diagnostic_message_limit - 1, 'x');
    long_message += "日本";
    const std::string body =
        R"({"uri":"u","diagnostics":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},)"
        R"("message":")" +
        long_message + R"(","code":12,"source":1}]})";
    const auto params = core::parse_json(body);
    const auto published = core::lsp_published_diagnostics(
        core::LspNotification{std::string(core::lsp_publish_diagnostics_method), params.value()},
        text, core::LspPositionEncoding::utf16);
    expect(published.has_value() && published.value().diagnostics.size() == 1 &&
               published.value().diagnostics.front().message ==
                   std::string(core::lsp_diagnostic_message_limit - 1, 'x') &&
               published.value().diagnostics.front().code == "12" &&
               published.value().diagnostics.front().source.empty(),
           "a long message is cut on a code point; an integer code reads in decimal and a "
           "non-string source is empty");
}

void verify_tables()
{
    expect(core::vim_action_for(U']') == core::VimAction::prefix_bracket_forward &&
               core::vim_action_for(U'[') == core::VimAction::prefix_bracket_back &&
               core::vim_g_action_for(U'l') == core::VimAction::language_diagnostic,
           "] and [ are separate prefixes and gl is a row of the g table");
    const core::KeyChord f8{false, false, core::OperationKey::f8};
    const core::KeyChord shift_f8{false, true, core::OperationKey::f8};
    expect(core::operation_for(f8, core::EditMode::ordinary) ==
                   core::EditorOperation::next_diagnostic &&
               core::operation_for(f8, core::EditMode::vim) ==
                   core::EditorOperation::next_diagnostic &&
               core::operation_for(shift_f8, core::EditMode::ordinary) ==
                   core::EditorOperation::previous_diagnostic &&
               core::key_chord_label(f8) == "F8" && core::key_chord_label(shift_f8) == "Shift+F8",
           "F8 and Shift+F8 walk the diagnostics in both modes");
    expect(
        !core::shown_chord(core::EditorOperation::show_diagnostic, core::EditMode::ordinary)
                .has_value() &&
            !core::shown_chord(core::EditorOperation::list_diagnostics, core::EditMode::vim)
                 .has_value() &&
            core::operation_available(core::EditorOperation::list_diagnostics, core::EditMode::vim),
        "showing and listing have no key but are in the F1 list of both modes");
    const auto query = core::palette_query_of("^main");
    expect(query.scope == core::PaletteScope::diagnostics && query.query == "main" &&
               core::palette_input_for(core::PaletteQuery{core::PaletteScope::diagnostics, ""}) ==
                   "^",
           "^ opens the diagnostics source of the Ctrl+P list");
    const std::vector<core::CommandChoice> entries{
        core::CommandChoice{core::DisplayText::parse("E a.cpp:1:1").value(),
                            "0",
                            core::CommandChoiceKind::land,
                            std::nullopt,
                            core::PaletteOrigin::diagnostic,
                            std::nullopt,
                            {}}};
    expect(core::listed_positions(entries, core::PaletteScope::files, "").empty() &&
               core::listed_positions(entries, core::PaletteScope::diagnostics, "") ==
                   std::vector<std::size_t>{0},
           "diagnostics are not file candidates and show only under ^");
}

// engine は `]d` `[d` `gl` に失敗の歩のまま効果を添える（NORMAL・オペレータなし・`gl`
// は回数なし）。
void verify_engine_effects()
{
    const auto buffer = core::TextBuffer::from_utf8("ab\ncd").value();
    const core::VimEditorView view{buffer, core::collapsed_at(Offset{0}),
                                   core::VimViewport{core::LineNumber{1}, 8, 0}};
    const auto stepped = [&](std::string_view keys)
    {
        core::VimState state = empty_vim_state();
        core::VimStep step{state, core::VimNoEffect{}};
        for (const char key : keys)
        {
            step = core::vim_step(state, view,
                                  core::VimKey{core::VimCharacter{static_cast<char32_t>(key)}});
            state = step.next;
        }
        return step;
    };
    const auto effect_of = [&](std::string_view keys) -> std::optional<core::VimDiagnosticCommand>
    {
        const core::VimStep step = stepped(keys);
        const auto *found = std::get_if<core::VimDiagnosticCommand>(&step.effect);
        return found == nullptr ? std::nullopt : std::optional{*found};
    };
    const auto next = effect_of("]d");
    const auto previous = effect_of("3[d");
    const auto shown = effect_of("gl");
    expect(next.has_value() && next.value().command == core::DiagnosticCommand::next &&
               next.value().count == 1 && previous.has_value() &&
               previous.value().command == core::DiagnosticCommand::previous &&
               previous.value().count == 3 && shown.has_value() &&
               shown.value().command == core::DiagnosticCommand::show,
           "]d [d and gl ask the controller with the count");
    expect(stepped("]d").failure == core::VimRepeatFailure::refused &&
               !stepped("]d").next.input_wait.has_value(),
           "the step itself stays the refused step Vim takes for ]d (no text, no caret)");
    expect(!effect_of("d]d").has_value() && !effect_of("v]d").has_value() &&
               !effect_of("2gl").has_value() && !effect_of("]x").has_value(),
           "an operator, VISUAL, a counted gl and other bracket keys add nothing");
}

// ---------------------------------------------------------------- controller の道

// F8 / Shift+F8
// は次・前の診断の始まりへ跳び、跳んだ先の文の全部を札で見せる。次の操作で札は消える。
void verify_ordinary_jumps()
{
    Editing editing(lsp_on());
    diagnosed(editing);
    const auto first = commanded(editing, core::DiagnosticCommand::next);
    expect(caret_on(first, 1, 5) &&
               tip_of(editing) == "bad main\nsecond line\nclangd(42)\n\na hint",
           "F8 lands on the next diagnostic and shows the whole message with its source and code");
    const auto second = commanded(editing, core::DiagnosticCommand::next);
    expect(caret_on(second, 2, 5) && tip_of(editing) == "unused x",
           "the next F8 moves to the following position");
    expect(caret_on(commanded(editing, core::DiagnosticCommand::next), 1, 5),
           "F8 after the last diagnostic wraps to the first");
    expect(caret_on(commanded(editing, core::DiagnosticCommand::previous), 2, 5),
           "Shift+F8 walks back and wraps to the last");
    static_cast<void>(editing.controller().apply(
        app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{1}},
                        core::SelectionAnchoring::collapse}));
    expect(tip_of(editing) == "<none>", "the next operation drops the tip like a hover");
}

void verify_show_and_absent()
{
    Editing editing(lsp_on());
    diagnosed(editing);
    const auto away = commanded(editing, core::DiagnosticCommand::show);
    expect(tip_of(editing) == "<none>" &&
               message_of(away) == core::ui_text(core::UiMessage::notice_no_diagnostic_here,
                                                 editing.controller().display_language()),
           "showing off a diagnostic says so in one line");
    static_cast<void>(editing.controller().apply(
        app::PlaceCaret{core::TextPosition{core::LineNumber{2}, core::Column{5}},
                        core::SelectionAnchoring::collapse}));
    static_cast<void>(commanded(editing, core::DiagnosticCommand::show));
    expect(tip_of(editing) == "unused x", "showing on a diagnostic opens its tip");

    Editing plain;
    plain.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    applied(plain.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    const auto none = commanded(plain, core::DiagnosticCommand::next);
    expect(caret_on(none, 1, 1) &&
               message_of(none) == core::ui_text(core::UiMessage::notice_no_diagnostics,
                                                 plain.controller().display_language()),
           "without diagnostics F8 stays and says there are none");
}

// Vim の `]d` `[d` はジャンプの列に載り（`''` で戻る）、回数を持ち、本文を変えない。
void verify_vim_jumps()
{
    Editing editing(lsp_on());
    diagnosed(editing);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "]d");
    expect(caret_on(editing.controller().frame(), 1, 5) &&
               tip_of(editing) == "bad main\nsecond line\nclangd(42)\n\na hint",
           "]d lands on the next diagnostic with its tip");
    vim_replay(editing.controller(), "''");
    expect(caret_on(editing.controller().frame(), 1, 1), "'' goes back to before the jump");
    vim_replay(editing.controller(), "2]d");
    expect(caret_on(editing.controller().frame(), 2, 5), "a count walks that many diagnostics");
    vim_replay(editing.controller(), "[d");
    expect(caret_on(editing.controller().frame(), 1, 5), "[d walks back");
    vim_replay(editing.controller(), "ggd]dv]d<Esc>gl");
    expect(whole_vim_body(editing.controller()) == source_text && tip_of(editing) == "<none>",
           "the keys never change the text and gl off a diagnostic shows no tip");
    vim_replay(editing.controller(), "]dlgl");
    expect(tip_of(editing) == "bad main\nsecond line\nclangd(42)\n\na hint",
           "gl inside a diagnostic shows its tip");
}

// 一覧は `^` で開き、アクティブな文書の診断が先・行 → 桁 → 重さ。選ぶとその位置へ跳ぶ。ほかの
// タブの診断は、そのタブへ切り替えてから跳ぶ。
void verify_list()
{
    Editing editing(lsp_on());
    diagnosed(editing);
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(second_path).value()});
    editing.language_server().publish(
        second_uri, 0,
        R"([{"range":{"start":{"line":0,"character":4},"end":{"line":0,"character":5}},"severity":1,"message":"y?"}])");
    static_cast<void>(editing.controller().apply(app::WorkCompleted{}));
    const auto listed = commanded(editing, core::DiagnosticCommand::list);
    expect(listed.command_palette.has_value() && listed.command_palette.value().total == 4,
           "the list opens with every diagnostic of the open tabs");
    if (!listed.command_palette.has_value() || listed.command_palette.value().rows.size() < 4)
    {
        return;
    }
    const auto &rows = listed.command_palette.value().rows;
    const std::optional<core::DisplayText> &detail = rows.at(1).detail;
    expect(rows.at(0).label.text() == "E two.cpp:1:5" &&
               rows.at(1).label.text() == "E main.cpp:1:5" &&
               rows.at(2).label.text() == "H main.cpp:1:5" &&
               rows.at(3).label.text() == "W main.cpp:2:5" && detail.has_value() &&
               detail.value().text() == "bad main",
           "the active document comes first; each row is mark, file:line:column and the first "
           "line");
    const auto landed = editing.controller().apply_frame(app::ActivateCommandChoice{3});
    expect(!landed.command_palette.has_value() && landed.active_tab == 0 && caret_on(landed, 2, 5),
           "choosing a diagnostic of another tab switches to it and lands on the diagnostic");
}
} // namespace

void verify_diagnostic_jump_scope()
{
    verify_landing();
    verify_tip_text();
    verify_parsed_fields();
    verify_tables();
    verify_engine_effects();
    verify_ordinary_jumps();
    verify_show_and_absent();
    verify_vim_jumps();
    verify_list();
}
} // namespace nenenib::tests
