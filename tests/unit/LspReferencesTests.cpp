// scope `--lsp-references` の契約（#1947・ADR 0191 の決定 4）。言語サーバーの参照の一覧: Vim の
// `grr`・通常モードの Shift+F12・操作「参照の一覧」・Ctrl+P の一覧の記号 `&`。core の表、engine の
// 効果、controller の道を替え玉 ScriptedLanguageServer で確かめる。
#include "ActivateCommandChoice.hpp"
#include "CancelCommand.hpp"
#include "CloseTab.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "DisplayText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FileUri.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "KeyChord.hpp"
#include "LanguageQueryKind.hpp"
#include "LineNumber.hpp"
#include "LspAnswerFailure.hpp"
#include "LspAnswers.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "OperationKey.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "ServerExited.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimAction.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimKeyTable.hpp"
#include "VimLanguageQuery.hpp"
#include "VimNoEffect.hpp"
#include "VimRepeatFailure.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VisibleLines.hpp"
#include "WindowDeactivated.hpp"
#include "WorkCompleted.hpp"

#include <cstdint>
#include <expected>
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
using core::Offset;

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view second_path = "C:\\work\\two.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view source_text = "int main() {}\nint x = main();\n";
constexpr std::string_view second_text = "int y;\n\n int z = main();\n";
constexpr std::string_view references_method = "textDocument/references";
// main.cpp の 2 つ（宣言と呼び出し）と、開いていない two.cpp の 1 つ。
constexpr std::string_view three_references =
    R"([{"uri":"file:///C:/work/main.cpp","range":{"start":{"line":0,"character":4},"end":{"line":0,"character":8}}},)"
    R"({"uri":"file:///C:/work/main.cpp","range":{"start":{"line":1,"character":8},"end":{"line":1,"character":12}}},)"
    R"({"uri":"file:///C:/work/two.cpp","range":{"start":{"line":2,"character":9},"end":{"line":2,"character":13}}}])";

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
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

// main.cpp と two.cpp を置き、main.cpp を開いて capabilities で `initialize` に答える。
void started_with(Editing &editing, std::string_view capabilities, std::string_view main_text,
                  std::string_view two_text)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string(main_text)});
    editing.files().hold_at(std::string(second_path), Bytes{std::string(two_text)});
    static_cast<void>(editing.controller().apply(app::VisibleLines{4}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                           std::string{capabilities} + "}}"}));
}

void started(Editing &editing, std::string_view capabilities)
{
    started_with(editing, capabilities, source_text, second_text);
}

void referencing(Editing &editing)
{
    started(editing, R"({"textDocumentSync":2,"referencesProvider":true})");
}

app::EditorFrame asked(Editing &editing)
{
    return editing.controller().apply_frame(
        app::QueryLanguage{core::LanguageQueryKind::references});
}

app::EditorFrame answered(Editing &editing, std::string_view result)
{
    const auto ids = editing.language_server().requests(references_method);
    editing.language_server().answer(ids.empty() ? -1 : ids.back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] std::string notice(Editing &editing, core::UiMessage message)
{
    return std::string(core::ui_text(message, editing.controller().display_language()));
}

// ---------------------------------------------------------------- core の表と engine

void verify_tables()
{
    expect(core::vim_g_action_for(U'r') == core::VimAction::prefix_g_references,
           "gr is a row of the g table");
    const core::KeyChord shift_f12{false, true, core::OperationKey::f12};
    expect(core::operation_for(shift_f12, core::EditMode::ordinary) ==
                   core::EditorOperation::list_references &&
               core::operation_for(shift_f12, core::EditMode::vim) ==
                   core::EditorOperation::list_references &&
               core::operation_for(core::KeyChord{false, false, core::OperationKey::f12},
                                   core::EditMode::ordinary) ==
                   core::EditorOperation::go_to_definition &&
               core::key_chord_label(shift_f12) == "Shift+F12",
           "Shift+F12 lists the references in both modes and F12 still goes to the definition");
    const auto query = core::palette_query_of("&main");
    expect(query.scope == core::PaletteScope::references && query.query == "main" &&
               core::palette_input_for(core::PaletteQuery{core::PaletteScope::references, ""}) ==
                   "&",
           "& opens the references source of the Ctrl+P list");
    const std::vector<core::CommandChoice> entries{
        core::CommandChoice{core::DisplayText::parse("a.cpp:1:1").value(),
                            "0",
                            core::CommandChoiceKind::land,
                            std::nullopt,
                            core::PaletteOrigin::reference,
                            std::nullopt,
                            {}}};
    expect(core::listed_positions(entries, core::PaletteScope::files, "").empty() &&
               core::listed_positions(entries, core::PaletteScope::references, "") ==
                   std::vector<std::size_t>{0} &&
               core::listed_positions(entries, core::PaletteScope::diagnostics, "").empty(),
           "references are not file candidates and show only under &");
}

// engine の `grr` は表に行が無かったときと同じ失敗の歩に、参照の問いの効果だけを添える。
void verify_engine_effects()
{
    const auto buffer = core::TextBuffer::from_utf8("ab\ncd").value();
    const core::VimEditorView view{buffer, core::collapsed_at(Offset{0}),
                                   core::VimViewport{core::LineNumber{1}, 8, 0}};
    const auto stepped = [&](const std::vector<core::VimKey> &keys)
    {
        core::VimState state = empty_vim_state();
        core::VimStep step{state, core::VimNoEffect{}};
        for (const core::VimKey &key : keys)
        {
            step = core::vim_step(state, view, key);
            state = step.next;
        }
        return step;
    };
    const auto typed = [](std::string_view text)
    {
        std::vector<core::VimKey> keys;
        for (const char key : text)
        {
            keys.emplace_back(core::VimCharacter{static_cast<char32_t>(key)});
        }
        return keys;
    };
    const auto queried = [&](std::string_view text)
    {
        const core::VimStep step = stepped(typed(text));
        const auto *found = std::get_if<core::VimLanguageQuery>(&step.effect);
        return found != nullptr && found->kind == core::LanguageQueryKind::references;
    };
    expect(queried("grr"), "grr asks for the references");
    const core::VimStep waiting = stepped(typed("gr"));
    expect(waiting.next.input_wait.has_value() && !waiting.failure.has_value(),
           "gr waits for one more key");
    const core::VimStep grr = stepped(typed("grr"));
    expect(grr.failure == core::VimRepeatFailure::refused && !grr.next.input_wait.has_value(),
           "the step itself stays the refused step of a key without a row (no text, no caret)");
    expect(!queried("grx") && stepped(typed("grx")).failure == core::VimRepeatFailure::refused &&
               !queried("dgrr") && !queried("vgrr") && !queried("2grr"),
           "another key fails; an operator, VISUAL and a count add nothing");
    std::vector<core::VimKey> escaped = typed("gr");
    escaped.emplace_back(core::VimSpecialKey::escape);
    const core::VimStep cancelled = stepped(escaped);
    expect(!cancelled.failure.has_value() && !cancelled.next.input_wait.has_value(),
           "Esc after gr cancels without failing");
}

// 実物の clangd（Build Tools の LLVM）の答えの形（D:/NeNeNib/scripts/20261122/lsp-s6 の probe で
// 2026-11-22 に取った値）は定義と同じ Location[] で、同じ読みが通る。
void verify_clangd_shape()
{
    const auto parsed = core::parse_json(
        R"([{"range":{"end":{"character":7,"line":0},"start":{"character":4,"line":0}},"uri":"file:///D:/NeNeNib/work/main.cpp"},)"
        R"({"range":{"end":{"character":14,"line":6},"start":{"character":11,"line":6}},"uri":"file:///D:/NeNeNib/work/main.cpp"}])");
    const auto locations = parsed.has_value()
                               ? core::lsp_locations(parsed.value())
                               : std::unexpected{core::LspAnswerFailure::invalid_result};
    expect(locations.has_value() && locations.value().size() == 2 &&
               locations.value().at(1).range.start.line == 6 &&
               locations.value().at(1).range.start.character == 11 &&
               core::file_path_of_uri(locations.value().front().uri).has_value(),
           "clangd's reference answer reads as the same locations as a definition");
}

// ---------------------------------------------------------------- controller の道

// 頼むのはキャレットの位置と includeDeclaration。答えは `&` の一覧になり、開いているタブの候補は
// 行の文を持ち、開いていないファイルは持たない。選ぶと開いて跳ぶ。
void verify_list_and_landing()
{
    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(asked(editing));
    const auto &sent = editing.language_server().sent();
    expect(contains(sent.back(), R"("method":"textDocument/references")") &&
               contains(
                   sent.back(),
                   R"("position":{"line":0,"character":0},"context":{"includeDeclaration":true})"),
           "the request names the caret and includes the declaration");
    const auto listed = answered(editing, three_references);
    expect(listed.command_palette.has_value() && listed.command_palette.value().total == 3,
           "the answer opens the & list with every reference");
    if (!listed.command_palette.has_value() || listed.command_palette.value().rows.size() < 3)
    {
        return;
    }
    const auto &rows = listed.command_palette.value().rows;
    const std::optional<core::DisplayText> &first = rows.at(0).detail;
    expect(rows.at(0).label.text() == "main.cpp:1:5" && first.has_value() &&
               first.value().text() == "int main() {}" &&
               rows.at(1).label.text() == "main.cpp:2:9" &&
               rows.at(2).label.text() == "two.cpp:3" && !rows.at(2).detail.has_value(),
           "open tabs show the line; a file not open shows only file:line");
    const auto landed = editing.controller().apply_frame(app::ActivateCommandChoice{2});
    expect(!landed.command_palette.has_value() && caret_on(landed, 3, 10),
           "choosing a reference in a file not open opens it and lands on the reference");
    static_cast<void>(editing.controller().apply(app::SwitchTab{0}));
    static_cast<void>(
        editing.controller().apply_frame(app::QueryLanguage{core::LanguageQueryKind::references}));
    static_cast<void>(answered(editing, three_references));
    const auto same = editing.controller().apply_frame(app::ActivateCommandChoice{1});
    expect(!same.command_palette.has_value() && same.active_tab == 0 && caret_on(same, 2, 9),
           "choosing a reference in the active document moves the caret");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "''");
    expect(caret_on(editing.controller().frame(), 1, 1), "'' goes back to before the jump");
}

// 能力の無いサーバー・動いていない文書・空の答え・待っている間の答え。
void verify_absent_and_stale()
{
    Editing plain(lsp_on());
    started(plain, R"({"textDocumentSync":2})");
    const std::size_t before = plain.language_server().sent().size();
    const auto unsupported = asked(plain);
    expect(plain.language_server().sent().size() == before &&
               message_of(unsupported) ==
                   notice(plain, core::UiMessage::notice_references_not_supported),
           "a server without referencesProvider is not asked and says so");

    Editing off;
    off.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    applied(off.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    expect(message_of(asked(off)) ==
               notice(off, core::UiMessage::notice_language_server_not_running),
           "without a running server the list says the server is not running");

    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(asked(editing));
    const auto empty = answered(editing, "[]");
    expect(!empty.command_palette.has_value() &&
               message_of(empty) == notice(editing, core::UiMessage::notice_references_not_found),
           "an empty answer opens nothing and says so");
    static_cast<void>(asked(editing));
    applied(editing.controller(), app::InsertText{"x"});
    expect(!answered(editing, three_references).command_palette.has_value(),
           "an answer for an older revision is dropped");
}

// Vim の `grr` も同じ問い。
void verify_vim_keys()
{
    Editing editing(lsp_on());
    referencing(editing);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    const std::size_t before = editing.language_server().requests(references_method).size();
    vim_replay(editing.controller(), "grr");
    expect(editing.language_server().requests(references_method).size() == before + 1 &&
               whole_vim_body(editing.controller()) == source_text,
           "grr asks for the references and leaves the text");
    expect(answered(editing, three_references).command_palette.has_value(),
           "the answer to grr opens the list");
}
// 1 件の参照の場所の JSON。
[[nodiscard]] std::string reference_at(std::string_view uri, std::size_t line,
                                       std::size_t character)
{
    const std::string start =
        R"({"line":)" + std::to_string(line) + R"(,"character":)" + std::to_string(character) + "}";
    return R"({"uri":")" + std::string{uri} + R"(","range":{"start":)" + start + R"(,"end":)" +
           start + "}}";
}

// 受け取りで 1,000 件に切り、切ったことを 1 行知らせる。`file:` でない場所は外す（#1954 の
// レビュー R2・R4・L2）。
void verify_limit_and_foreign()
{
    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(asked(editing));
    std::string many = "[" + reference_at("untitled:Untitled-1", 0, 0);
    for (std::size_t index = 0; index < 1001; ++index)
    {
        many += "," + reference_at("file:///C:/work/main.cpp", 0, 4);
    }
    many += "]";
    const auto listed = answered(editing, many);
    expect(listed.command_palette.has_value() && listed.command_palette.value().total == 999,
           "the answer is cut to 1,000 places and the place that is not a file is left out");
    expect(message_of(listed) == core::ui_text_filled(core::UiMessage::notice_references_first_of,
                                                      editing.controller().display_language(),
                                                      {"1000", "1002"}),
           "a cut answer says how many places it had");
}

// 位置の単位は答えた席の単位。開いているタブは本文で行と桁を写し、開いていないファイルは桁を
// 出さない（答えの単位では複数バイトの行でずれる・#1955・跳んだ先は本文で写すので正しい）。
void verify_encodings_and_wide_lines()
{
    constexpr std::string_view wide_main = "int main() {}\n/*\xE3\x81\x82\xE3\x81\x84*/ main();\n";
    constexpr std::string_view wide_two = "int y;\n\n/*\xE3\x81\x82\xE3\x81\x84*/ main();\n";
    Editing editing(lsp_on());
    started_with(editing, R"({"textDocumentSync":2,"referencesProvider":true})", wide_main,
                 wide_two);
    static_cast<void>(asked(editing));
    const auto listed =
        answered(editing, "[" + reference_at("file:///C:/work/main.cpp", 1, 7) + "," +
                              reference_at("file:///C:/work/two.cpp", 2, 7) + "]");
    if (!listed.command_palette.has_value() || listed.command_palette.value().rows.size() < 2)
    {
        expect(false, "the wide lines list both references");
        return;
    }
    const auto &rows = listed.command_palette.value().rows;
    expect(rows.at(0).label.text() == "main.cpp:2:8" && rows.at(1).label.text() == "two.cpp:3",
           "the open tab counts the column in the text and the file not open shows no column");
    expect(caret_on(editing.controller().apply_frame(app::ActivateCommandChoice{1}), 3, 8),
           "the reference in the file not open lands on the name after opening it");

    Editing bytes(lsp_on());
    started_with(bytes,
                 R"({"positionEncoding":"utf-8","textDocumentSync":2,"referencesProvider":true})",
                 wide_main, wide_two);
    static_cast<void>(asked(bytes));
    static_cast<void>(answered(bytes, "[" + reference_at("file:///C:/work/main.cpp", 1, 11) + "]"));
    expect(caret_on(bytes.controller().apply_frame(app::ActivateCommandChoice{0}), 2, 8),
           "a server that counts UTF-8 bytes lands on the same name");
}

// `grr` は頼めたら成功の歩に読み替わり、マクロの続きが走る（#1954 のレビュー R4）。
void verify_grr_keeps_the_macro()
{
    Editing editing(lsp_on());
    started_with(editing, R"({"textDocumentSync":2,"referencesProvider":true})",
                 "int main() {}\nint x;\nint y;\nint z;\n", second_text);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "qagrrjqgg@a");
    expect(caret_on(editing.controller().frame(), 2, 1),
           "the j after grr in the macro runs when it is replayed");
}

// 一覧を開いた後に本文を短くしてから `&` を開き直して選んでも、本文の外へは跳ばない。答えを頼んだ
// 文書を閉じたら控えも捨てる（#1954 のレビュー R1・R4）。
void verify_reopen_after_edit()
{
    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, three_references));
    applied(editing.controller(), app::CancelCommand{});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    vim_replay(editing.controller(), "jdG");
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    const auto reopened = editing.controller().apply_frame(app::CommandText{"&"});
    expect(reopened.command_palette.has_value() && reopened.command_palette.value().total == 3,
           "& lists the last references again");
    const auto landed = editing.controller().apply_frame(app::ActivateCommandChoice{1});
    expect(!landed.command_palette.has_value() && landed.caret.position.line.value == 1,
           "a reference past the shortened text lands inside the text");
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    const auto still = editing.controller().apply_frame(app::CommandText{"&"});
    expect(still.command_palette.has_value() && still.command_palette.value().total == 3,
           "the references stay while the asking document is open");
    applied(editing.controller(), app::CancelCommand{});
}

// 窓が非アクティブの間に届いた答えは一覧を開かずに控える（#1954 のレビュー R5）。
void verify_inactive_window()
{
    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(asked(editing));
    static_cast<void>(editing.controller().apply(app::WindowDeactivated{}));
    expect(!answered(editing, three_references).command_palette.has_value(),
           "an answer that arrives while the window is inactive opens nothing");
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    const auto held = editing.controller().apply_frame(app::CommandText{"&"});
    expect(held.command_palette.has_value() && held.command_palette.value().total == 3,
           "the held answer shows under &");
}

// 答えを頼んだ文書を閉じたら控えを捨てる（#1954 のレビュー R1）。
void verify_forgotten_on_close()
{
    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(asked(editing));
    static_cast<void>(answered(editing, three_references));
    static_cast<void>(editing.controller().apply_frame(app::ActivateCommandChoice{2}));
    applied(editing.controller(), app::CloseTab{0});
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    const auto after = editing.controller().apply_frame(app::CommandText{"&"});
    expect(!after.command_palette.has_value() || after.command_palette.value().total == 0,
           "closing the asking document forgets its references");
}

// `&` の一覧の候補の数（一覧が開いていなければ値なし）。
[[nodiscard]] std::optional<std::size_t> listed_references(Editing &editing)
{
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    const auto frame = editing.controller().apply_frame(app::CommandText{"&"});
    applied(editing.controller(), app::CancelCommand{});
    return frame.command_palette.has_value() ? std::optional{frame.command_palette.value().total}
                                             : std::nullopt;
}

// 控えを捨てるほかの 3 つの道（#1958 のレビュー R3・a-L2）: 言語を変えた・名前を付けて保存した・
// 言語サーバーが落ちた。どれも言語サーバーが文書を閉じる道から捨てる。
void verify_forgotten_on_other_closes()
{
    const auto held = [](Editing &editing)
    {
        referencing(editing);
        static_cast<void>(asked(editing));
        static_cast<void>(answered(editing, three_references));
        applied(editing.controller(), app::CancelCommand{});
        return listed_references(editing) == std::optional<std::size_t>{3};
    };
    Editing language(lsp_on());
    const bool language_held = held(language);
    static_cast<void>(run_ex(language.controller(), "set ft=python"));
    expect(language_held && listed_references(language) == std::optional<std::size_t>{0},
           "changing the language forgets the references");

    Editing renamed(lsp_on());
    const bool renamed_held = held(renamed);
    static_cast<void>(run_ex(renamed.controller(), "saveas C:\\work\\renamed.cpp"));
    expect(renamed_held && listed_references(renamed) == std::optional<std::size_t>{0},
           "saving under another name forgets the references");

    Editing exited(lsp_on());
    const bool exited_held = held(exited);
    static_cast<void>(delivered(exited, app::ServerExited{1}));
    expect(exited_held && listed_references(exited) == std::optional<std::size_t>{0},
           "a server that stops forgets the references");
}

// 一覧の下の案内の欄に何を頼んだ答えかを出し、頼んでいない `&` は頼み方の 1 行（#1958 の決め
// 2・3）。 当たりを作るのは `&` を見せるときだけ（レビュー R1 の契約）。
void verify_subject_empty_line_and_lazy_hits()
{
    Editing editing(lsp_on());
    referencing(editing);
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    const auto empty = editing.controller().apply_frame(app::CommandText{"&"});
    const auto line =
        empty.command_palette.has_value() ? empty.command_palette.value().empty_line : std::nullopt;
    expect(line.has_value() && line.value().text() ==
                                   core::ui_text_filled(core::UiMessage::palette_empty_references,
                                                        editing.controller().display_language(),
                                                        {"Shift+F12", "grr"}),
           "& before asking says how to ask, with the key from the operation table");
    applied(editing.controller(), app::CancelCommand{});
    static_cast<void>(asked(editing));
    const std::size_t before = editing.controller().listed_hits_made();
    const auto listed = answered(editing, three_references);
    const auto subject =
        listed.command_palette.has_value() ? listed.command_palette.value().subject : std::nullopt;
    expect(subject.has_value() && contains(subject.value().text(), "main.cpp:1"),
           "the references list says where they were asked");
    expect(editing.controller().listed_hits_made() == before + 3, "showing & makes each hit once");
    applied(editing.controller(), app::CancelCommand{});
    const std::size_t shown = editing.controller().listed_hits_made();
    static_cast<void>(editing.controller().apply(app::OpenCommandPalette{}));
    static_cast<void>(editing.controller().apply(app::CommandText{"#"}));
    static_cast<void>(editing.controller().apply(app::EditCommand{core::CommandEdit::backspace}));
    applied(editing.controller(), app::CancelCommand{});
    expect(editing.controller().listed_hits_made() == shown,
           "a Ctrl+P list under another mark makes no hit of the held references");
}
} // namespace

void verify_lsp_references_scope()
{
    verify_tables();
    verify_engine_effects();
    verify_clangd_shape();
    verify_list_and_landing();
    verify_absent_and_stale();
    verify_vim_keys();
    verify_limit_and_foreign();
    verify_encodings_and_wide_lines();
    verify_grr_keeps_the_macro();
    verify_reopen_after_edit();
    verify_inactive_window();
    verify_forgotten_on_close();
    verify_forgotten_on_other_closes();
    verify_subject_empty_line_and_lazy_hits();
}
} // namespace nenenib::tests
