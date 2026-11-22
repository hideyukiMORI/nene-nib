// scope `--lsp-signature` の契約（#1959・ADR 0191 の決定 6）。言語サーバーの引数の情報: 挿入中に
// `triggerCharacters` を打ったときの自動の頼み・操作「引数の情報を表示」・札の出る間と消える条件。
// core の読みと controller の道を替え玉 ScriptedLanguageServer で確かめる。
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "LanguageQueryKind.hpp"
#include "LspAnswerFailure.hpp"
#include "LspSignature.hpp"
#include "LspSignatureHelp.hpp"
#include "MoveCaret.hpp"
#include "OpenDocument.hpp"
#include "OperationBindings.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
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
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view source_text = "int add(int a, int b);\nint main() {\n  \n}\n";
constexpr std::string_view signature_method = "textDocument/signatureHelp";
// 実物の clangd（D:/NeNeNib/scripts/20261122/lsp-s6 の probe）の能力と答えの形。
constexpr std::string_view clangd_abilities =
    R"j({"textDocumentSync":2,"signatureHelpProvider":{"triggerCharacters":["(",")","{","}","<",">",","]}})j";
constexpr std::string_view clangd_answer =
    R"({"activeParameter":1,"activeSignature":0,"signatures":[{"label":"add(int a, int b) -> int","parameters":[{"label":[4,9]},{"label":[11,16]}]}]})";

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
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

// main.cpp を開いて capabilities で `initialize` に答え、キャレットを 3 行目の終わりへ置く。
void started(Editing &editing, std::string_view capabilities)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    static_cast<void>(editing.controller().apply(app::VisibleLines{6}));
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    static_cast<void>(delivered(editing, app::ServerStarted{}));
    static_cast<void>(delivered(
        editing,
        app::ServerMessage{std::string{R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":)"} +
                           std::string{capabilities} + "}}"}));
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse});
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse});
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::line_end, core::SelectionAnchoring::collapse});
}

[[nodiscard]] std::size_t asked_count(Editing &editing)
{
    return editing.language_server().requests(signature_method).size();
}

app::EditorFrame answered(Editing &editing, std::string_view result)
{
    const auto ids = editing.language_server().requests(signature_method);
    editing.language_server().answer(ids.empty() ? -1 : ids.back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

app::EditorFrame typed(Editing &editing, std::string_view text)
{
    return editing.controller().apply_frame(app::InsertText{std::string{text}});
}

[[nodiscard]] std::optional<core::LspSignature> read(std::string_view json)
{
    const auto parsed = core::parse_json(json);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    return core::lsp_signature_help(parsed.value()).value_or(std::nullopt);
}

// ---------------------------------------------------------------- core の読み

// 今の引数は UTF-16 の範囲か label の中の文字列。署名の activeParameter が全体より先。
void verify_reading()
{
    const auto clangd = read(clangd_answer);
    expect(clangd.has_value() && clangd.value().label == "add(int a, int b) -> int" &&
               clangd.value().active_begin == 11 && clangd.value().active_end == 16,
           "the clangd answer highlights the second parameter");
    const auto named = read(
        R"j({"activeParameter":0,"signatures":[{"label":"f(x, y)","activeParameter":1,"parameters":[{"label":"x"},{"label":"y"}]}]})j");
    expect(named.has_value() && named.value().active_begin == 5 && named.value().active_end == 6,
           "a string label is found in the signature and its own activeParameter wins");
    const auto wide = read(
        R"j({"signatures":[{"label":"g(\u3042 p, int q)","parameters":[{"label":[2,5]},{"label":[7,12]}]}],"activeParameter":1})j");
    expect(wide.has_value() && wide.value().label.substr(wide.value().active_begin,
                                                         wide.value().active_end -
                                                             wide.value().active_begin) == "int q",
           "UTF-16 offsets are mapped to bytes past a wide character");
    const auto null = core::parse_json("null");
    const auto empty = core::parse_json(R"({"signatures":[]})");
    const auto wrong = core::parse_json("[1]");
    expect(null.has_value() && !core::lsp_signature_help(null.value()).value().has_value() &&
               empty.has_value() && !core::lsp_signature_help(empty.value()).value().has_value() &&
               wrong.has_value() &&
               core::lsp_signature_help(wrong.value()).error() ==
                   core::LspAnswerFailure::invalid_result,
           "null and no signatures are no tip and another shape fails");
    const auto bare = read(R"j({"signatures":[{"label":"h()"}]})j");
    expect(bare.has_value() && bare.value().active_begin == 0 && bare.value().active_end == 0,
           "a signature without parameters highlights nothing");
    expect(std::ranges::none_of(core::operation_bindings,
                                [](const core::OperationBinding &binding)
                                {
                                    return binding.operation ==
                                               core::EditorOperation::show_signature_help &&
                                           binding.chord.has_value();
                                }),
           "showing the signature has no key (F1 list)");
}

// ---------------------------------------------------------------- controller の道

// `(` で頼み、札は打鍵で消えずに残り、`,` で頼み直す。`)`・Esc・行の移動で消える。
void verify_typing()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    const std::size_t before = asked_count(editing);
    // 窓は打った 1 文字ごとに InsertText を送る。
    static_cast<void>(typed(editing, "add"));
    static_cast<void>(typed(editing, "("));
    expect(asked_count(editing) == before + 1 &&
               contains(editing.language_server().sent().back(), R"j("triggerCharacter":"(")j") &&
               contains(editing.language_server().sent().back(), R"("isRetrigger":false)"),
           "typing ( asks for the signature");
    const auto shown = answered(editing, clangd_answer);
    expect(shown.signature.has_value() && shown.signature.value().active_begin == 11,
           "the answer shows the signature tip");
    const auto kept = typed(editing, "1");
    expect(kept.signature.has_value() && asked_count(editing) == before + 1,
           "typing an argument keeps the tip and asks nothing");
    static_cast<void>(typed(editing, ","));
    expect(asked_count(editing) == before + 2 &&
               contains(editing.language_server().sent().back(), R"("isRetrigger":true)"),
           "typing , asks again as a retrigger");
    const auto closed = typed(editing, ")");
    expect(!closed.signature.has_value(), "typing ) closes the tip");
    static_cast<void>(answered(editing, clangd_answer));
    expect(editing.controller().frame().signature.has_value(),
           "an answer after ) inside an outer call shows the outer signature again");
    applied(editing.controller(), app::CancelSelection{});
    expect(!editing.controller().frame().signature.has_value(), "Esc closes the tip");
    static_cast<void>(typed(editing, "("));
    static_cast<void>(answered(editing, clangd_answer));
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::previous_line, core::SelectionAnchoring::collapse});
    expect(!editing.controller().frame().signature.has_value(),
           "moving to another line closes the tip");
    static_cast<void>(typed(editing, "("));
    expect(!answered(editing, "null").signature.has_value(), "an empty answer shows no tip");
}

// 窓で打った鍵と同じ入口（accept(VimKeyPress)）で鍵を 1 つずつ送る。
void pressed(Editing &editing, std::string_view keys)
{
    for (const core::VimKey &key : vim_keys_of(keys))
    {
        static_cast<void>(editing.controller().apply(app::VimKeyPress{key}));
    }
}

// Vim の INSERT でも頼み、NORMAL へ抜けたら消える。頼むのは使う人が打った 1 文字が本文に入った
// ときだけで、`map` の右辺・録画の再生・`:normal` は頼まない（#1962 と同じ決め）。
void verify_vim_insert()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    const std::size_t before = asked_count(editing);
    pressed(editing, "aadd(");
    expect(asked_count(editing) == before + 1, "typing ( in INSERT asks for the signature");
    expect(answered(editing, clangd_answer).signature.has_value(), "the INSERT shows the tip");
    pressed(editing, "<Esc>");
    expect(!editing.controller().frame().signature.has_value(), "leaving INSERT closes the tip");
    const std::size_t normal = asked_count(editing);
    pressed(editing, "f(");
    expect(asked_count(editing) == normal, "a ( typed as a NORMAL argument asks nothing");
    static_cast<void>(run_ex(editing.controller(), "inoremap xx add("));
    pressed(editing, "axx<Esc>");
    expect(asked_count(editing) == normal, "a ( from the right side of a map asks nothing");
    pressed(editing, "qqa(<Esc>q");
    const std::size_t recorded = asked_count(editing);
    pressed(editing, "@q");
    static_cast<void>(run_ex(editing.controller(), "normal! a("));
    expect(recorded == normal + 1 && asked_count(editing) == recorded,
           "the ( typed while recording asks; the replay and :normal ask nothing");
}

// 契約: lsp がオフ・サーバーが能力を持たないとき、打鍵の道は言語サーバーに触れない（#1959）。
void verify_quiet_when_off_or_unable()
{
    Editing off(lsp_settings(false));
    off.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    applied(off.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    const std::size_t touches = off.language_server().touches();
    static_cast<void>(typed(off, "add(1, 2)"));
    expect(off.language_server().touches() == touches && asked_count(off) == 0,
           "with lsp off typing ( touches no language server");

    Editing unable(lsp_settings(true));
    started(unable, R"({"textDocumentSync":2})");
    const std::size_t sent = unable.language_server().sent().size();
    static_cast<void>(typed(unable, "("));
    const auto &after = unable.language_server().sent();
    expect(asked_count(unable) == 0 && after.size() == sent + 1 &&
               contains(after.back(), R"("method":"textDocument/didChange")"),
           "a server without signatureHelpProvider gets only the change");
}

// 操作「引数の情報を表示」は手で頼み、空なら 1 行知らせ、能力が無ければ「答えません」。
void verify_operation()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    const std::size_t before = asked_count(editing);
    static_cast<void>(
        editing.controller().apply(app::QueryLanguage{core::LanguageQueryKind::signature_help}));
    expect(asked_count(editing) == before + 1 &&
               contains(editing.language_server().sent().back(), R"("triggerKind":1)"),
           "the operation asks as invoked");
    const auto empty = answered(editing, "null");
    expect(!empty.signature.has_value() &&
               message_of(empty) == core::ui_text(core::UiMessage::notice_signature_not_found,
                                                  editing.controller().display_language()),
           "an empty answer to the operation says so");

    Editing unable(lsp_settings(true));
    started(unable, R"({"textDocumentSync":2})");
    const auto refused = unable.controller().apply_frame(
        app::QueryLanguage{core::LanguageQueryKind::signature_help});
    expect(message_of(refused) == core::ui_text(core::UiMessage::notice_signature_not_supported,
                                                unable.controller().display_language()),
           "a server without signatureHelpProvider says it does not answer");
}
} // namespace

void verify_lsp_signature_scope()
{
    verify_reading();
    verify_typing();
    verify_vim_insert();
    verify_quiet_when_off_or_unable();
    verify_operation();
}
} // namespace nenenib::tests
