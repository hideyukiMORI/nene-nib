// scope `--lsp-signature` の契約（#1959・ADR 0191 の決定 6）。言語サーバーの引数の情報: 挿入中に
// `triggerCharacters` を打ったときの自動の頼み・操作「引数の情報を表示」・札の出る間と消える条件。
// core の読みと controller の道を替え玉 ScriptedLanguageServer で確かめる。
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "CompletionFace.hpp"
#include "CompletionFaceRequest.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorIntent.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "HoverTip.hpp"
#include "InsertText.hpp"
#include "JsonParse.hpp"
#include "LanguageQueryKind.hpp"
#include "LayoutRect.hpp"
#include "LineView.hpp"
#include "LspAnswerFailure.hpp"
#include "LspSignature.hpp"
#include "LspSignatureHelp.hpp"
#include "MoveCaret.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenContextMenu.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "OperationBindings.hpp"
#include "QueryLanguage.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "SwitchTab.hpp"
#include "TabTipMeasure.hpp"
#include "TestSupport.hpp"
#include "TextViewGeometry.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WindowDeactivated.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
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
    R"j({"textDocumentSync":2,"hoverProvider":true,"signatureHelpProvider":{"triggerCharacters":["(",")","{","}","<",">",","]}})j";
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

// c-R3: 文字列の label は `(` の後から前の引数の終わりを起点に順に探す。activeParameter が引数の
// 数の外なら 0 番目（LSP 3.17）、null（3.18）なら強めない（署名の欄が全体より先）。
void verify_reading_parameters()
{
    const auto first = read(
        R"j({"activeParameter":0,"signatures":[{"label":"add(a, b)","parameters":[{"label":"a"},{"label":"b"}]}]})j");
    expect(first.has_value() && first.value().active_begin == 4 && first.value().active_end == 5,
           "a string label is looked for after the first ( and not in the function name");
    const auto second = read(
        R"j({"activeParameter":1,"signatures":[{"label":"aa(a, a)","parameters":[{"label":"a"},{"label":"a"}]}]})j");
    expect(second.has_value() && second.value().active_begin == 6 && second.value().active_end == 7,
           "the next string label is looked for after the end of the one before");
    const auto outside = read(
        R"j({"activeParameter":5,"signatures":[{"label":"f(x, y)","parameters":[{"label":[2,3]},{"label":[5,6]}]}]})j");
    expect(outside.has_value() && outside.value().active_begin == 2 &&
               outside.value().active_end == 3,
           "an activeParameter outside the parameters highlights the first one");
    const auto none = read(
        R"j({"activeParameter":null,"signatures":[{"label":"f(x, y)","parameters":[{"label":[2,3]},{"label":[5,6]}]}]})j");
    const auto own_none = read(
        R"j({"activeParameter":1,"signatures":[{"label":"f(x, y)","activeParameter":null,"parameters":[{"label":[2,3]},{"label":[5,6]}]}]})j");
    expect(none.has_value() && none.value().active_end == 0 && own_none.has_value() &&
               own_none.value().active_end == 0,
           "a null activeParameter highlights nothing and the signature's own null wins");
}

[[nodiscard]] bool overlapping(const core::LayoutRect &one, const core::LayoutRect &other)
{
    return one.left < other.right && other.left < one.right && one.top < other.bottom &&
           other.top < one.bottom;
}

// c-M1: 札は補完の一覧の箱を避ける。キャレットが窓の下の端（一覧は上へ回る）と上の端（一覧は下・
// 札は上に入らない）のどちらでも、両方が出ていて 2 つの矩形とキャレットが交わらない。
void verify_tip_avoids_completion()
{
    const core::LayoutRect area{0, 0, 800, 600};
    const core::TabTipMeasure text{200, 20, 96};
    const auto placed = [&](const core::LayoutRect &caret)
    {
        const auto face = core::completion_face_layout(
            core::CompletionFaceRequest{caret, caret.left, area, 8, 0, false, 96});
        return std::pair{face, core::signature_tip_rect(caret, area, text, face.panel)};
    };
    const core::LayoutRect bottom{100, 578, 102, 598};
    const auto [low_face, low_tip] = placed(bottom);
    expect(low_face.above && low_tip.has_value() && !overlapping(low_tip.value(), low_face.panel) &&
               !overlapping(low_tip.value(), bottom) && low_tip.value().bottom <= area.bottom &&
               low_tip.value().bottom <= low_face.panel.top,
           "at the bottom edge the tip goes above the completion list and covers no row");
    const core::LayoutRect top{100, 2, 102, 22};
    const auto [high_face, high_tip] = placed(top);
    expect(!high_face.above && high_tip.has_value() &&
               !overlapping(high_tip.value(), high_face.panel) &&
               !overlapping(high_tip.value(), top) && high_tip.value().top >= area.top,
           "at the top edge the tip goes below the completion list and covers no row");
    const core::LayoutRect middle{100, 300, 102, 320};
    expect(
        core::signature_tip_rect(middle, area, text, std::nullopt).value_or(core::LayoutRect{}) ==
            core::hover_tip_rect(middle, area, text),
        "without a completion list the tip sits where the hover tip sits");
    const core::LayoutRect cramped{0, 0, 800, 230};
    const core::LayoutRect low{100, 208, 102, 228};
    const auto squeezed = core::completion_face_layout(
        core::CompletionFaceRequest{low, low.left, cramped, 8, 0, false, 96});
    expect(!core::signature_tip_rect(low, cramped, text, squeezed.panel).has_value(),
           "when the tip fits nowhere beside the list the list wins and no tip is drawn");
    // #1968 のレビュー L-a:
    // 折り返した行の見えていない画面行（枠の上・下）のキャレットでは出さない。
    const core::LayoutRect pane{40, 0, 800, 600};
    expect(core::caret_rect_in_pane(middle, pane) &&
               !core::caret_rect_in_pane(core::LayoutRect{100, -18, 102, -2}, pane) &&
               !core::caret_rect_in_pane(core::LayoutRect{100, 602, 102, 618}, pane),
           "a caret row outside the pane draws no tip");
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
// typescript-language-server の形: `)` は retrigger だけ。
constexpr std::string_view retrigger_abilities =
    R"j({"textDocumentSync":2,"signatureHelpProvider":{"triggerCharacters":["(",",","<"],"retriggerCharacters":[")"]}})j";

// c-R1: `)` が retrigger だけのサーバーでも、入れ子の呼び出しを閉じた後に外側の札を頼み直す
// （札を消す前に「出ていたか」を見る）。
void verify_retrigger_close()
{
    Editing editing(lsp_settings(true));
    started(editing, retrigger_abilities);
    static_cast<void>(typed(editing, "f"));
    static_cast<void>(typed(editing, "("));
    static_cast<void>(answered(editing, clangd_answer));
    static_cast<void>(typed(editing, "g"));
    static_cast<void>(typed(editing, "("));
    static_cast<void>(answered(editing, clangd_answer));
    static_cast<void>(typed(editing, "1"));
    const std::size_t before = asked_count(editing);
    const auto closed = typed(editing, ")");
    expect(!closed.signature.has_value() && asked_count(editing) == before + 1 &&
               contains(editing.language_server().sent().back(), R"("isRetrigger":true)"),
           "closing f(g(1) with a retrigger-only ) asks again as a retrigger");
    expect(answered(editing, clangd_answer).signature.has_value(),
           "the answer brings the outer tip back");
}

// 札を出して返す（`(` と clangd の答え）。
void tip_shown(Editing &editing)
{
    static_cast<void>(typed(editing, "("));
    static_cast<void>(answered(editing, clangd_answer));
    expect(editing.controller().frame().signature.has_value(), "the tip is shown first");
}

// c-R4: 決定 6 の消える条件のうち、タブの切り替え・窓が非アクティブ・入力行・右クリックのメニュー・
// 設定・hover の札。照合で捨てた古い答えは今の札を残す（新しい札にしない）。
void verify_tip_lifetime()
{
    const auto closes = [](std::string_view what, const std::function<void(Editing &)> &act)
    {
        Editing editing(lsp_settings(true));
        started(editing, clangd_abilities);
        tip_shown(editing);
        act(editing);
        const std::string described = std::string{what} + " closes the tip";
        expect(!editing.controller().frame().signature.has_value(), described.c_str());
    };
    // タブの切り替え（#1968 のレビュー L-d）: 2 つめのタブを先に開いて main.cpp へ戻り、札を出して
    // から SwitchTab で替える（開く道で消えた札を後の SwitchTab が縛らないように）。
    {
        Editing editing(lsp_settings(true));
        started(editing, clangd_abilities);
        editing.files().hold_at("C:\\work\\other.cpp", Bytes{std::string{"int x;\n"}});
        applied(editing.controller(),
                app::OpenDocument{core::FilePath::parse("C:\\work\\other.cpp").value()});
        applied(editing.controller(), app::SwitchTab{0});
        tip_shown(editing);
        applied(editing.controller(), app::SwitchTab{1});
        expect(!editing.controller().frame().signature.has_value(),
               "switching to another tab closes the tip");
    }
    closes("the window going inactive", [](Editing &editing)
           { static_cast<void>(editing.controller().apply(app::WindowDeactivated{})); });
    closes("opening the Ctrl+P list", [](Editing &editing)
           { static_cast<void>(editing.controller().apply(app::OpenCommandPalette{})); });
    closes("opening the context menu",
           [](Editing &editing)
           {
               static_cast<void>(
                   editing.controller().apply(app::OpenContextMenu{10, 40, std::nullopt}));
           });
    closes("opening the settings", [](Editing &editing)
           { static_cast<void>(editing.controller().apply(app::OpenSettings{})); });
    closes("a hover tip",
           [](Editing &editing)
           {
               static_cast<void>(editing.controller().request_hover());
               const auto ids = editing.language_server().requests("textDocument/hover");
               editing.language_server().answer(ids.empty() ? -1 : ids.back(),
                                                R"j({"contents":"int add(int a, int b)"})j");
               static_cast<void>(editing.controller().apply(app::WorkCompleted{}));
           });

    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    tip_shown(editing);
    static_cast<void>(typed(editing, ","));
    static_cast<void>(typed(editing, "2"));
    const auto kept = answered(
        editing,
        R"({"activeParameter":0,"signatures":[{"label":"add(int a, int b) -> int","parameters":[{"label":[4,9]},{"label":[11,16]}]}]})");
    expect(kept.signature.has_value() && kept.signature.value().active_begin == 11,
           "an answer dropped by the context check keeps the current tip");
}

// c-R2': 通常モードでホイールを回してキャレットの行が見えなくなったら札を隠し、戻れば出す。
void verify_tip_scrolled_away()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    tip_shown(editing);
    static_cast<void>(editing.controller().apply(app::VisibleLines{1}));
    static_cast<void>(editing.controller().apply(app::ScrollLines{-2}));
    const auto away = editing.controller().frame();
    expect(!away.signature.has_value() &&
               std::ranges::none_of(away.lines, [&](const app::LineView &view)
                                    { return view.number == away.caret.position.line; }),
           "the tip hides while the caret line is scrolled out of view");
    static_cast<void>(editing.controller().apply(app::ScrollLines{2}));
    expect(editing.controller().frame().signature.has_value(),
           "the tip comes back when the caret line is in view again");
}

// #1968 のレビュー R-a: 格子表示の文書では行が空で画面行が grid に載る。札は格子の画面行の番号で
// 照合して出す。
void verify_tip_in_grid()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    static_cast<void>(
        editing.controller().apply(app::VisibleLines{6, core::TextViewGeometry{400, 20, 96}}));
    applied(editing.controller(), app::SelectDisplay{core::DisplayForm::grid});
    tip_shown(editing);
    const auto frame = editing.controller().frame();
    expect(frame.display == core::DisplayForm::grid && frame.lines.empty() && !frame.grid.empty(),
           "the document is drawn as a grid");
    expect(frame.signature.has_value(), "the tip shows on a grid document");
}

// #1968 のレビュー L-c: 貼り付けた `(` は打った文字ではないので頼まない（通常モードの Ctrl+V・
// Vim の `"+p`・INSERT の Ctrl-r `+`）。
void verify_paste_asks_nothing()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    editing.clipboard().hold(std::string{"add("});
    applied(editing.controller(), app::ClipboardAction{app::ClipboardOperation::paste});
    expect(asked_count(editing) == 0 && !editing.controller().frame().signature.has_value(),
           "pasting add( with Ctrl+V asks nothing");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    pressed(editing, "\"+p");
    expect(asked_count(editing) == 0, "pasting add( with \"+p asks nothing");
    pressed(editing, "a<C-r>+");
    expect(asked_count(editing) == 0 &&
               contains(editing.controller().text().line_text(core::LineNumber{3}), "add(add(add("),
           "inserting add( with Ctrl-r + in INSERT asks nothing");
}

// c-R5: 手で頼んだ答えが照合で捨てられたら印を下ろし、後の `(` の空の答えで知らせを出さない。
void verify_invoked_mark_dropped()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    static_cast<void>(
        editing.controller().apply(app::QueryLanguage{core::LanguageQueryKind::signature_help}));
    static_cast<void>(typed(editing, "1"));
    static_cast<void>(answered(editing, clangd_answer));
    static_cast<void>(typed(editing, "("));
    const auto empty = answered(editing, "null");
    expect(!empty.signature.has_value() && message_of(empty).empty(),
           "a typed ( after a dropped invoked answer shows no not-found notice");
}

// #1965 の入口の見分け（統合の後）: INSERT の 1 鍵の引数（Ctrl-r のレジスタの名前）として打った
// `(` は頼まない（Nib の INSERT の Ctrl+V は窓の OS の貼り付けで engine に届かない・ADR 0035 の
// 決定 9）。`inoremap jk <Esc>` の待ちの後にまとめて流れた `j(` の `(` は打った文字として頼む。
void verify_vim_typed_entry()
{
    Editing editing(lsp_settings(true));
    started(editing, clangd_abilities);
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    const std::size_t before = asked_count(editing);
    pressed(editing, "a<C-r>(");
    expect(asked_count(editing) == before, "a ( typed as the register name of Ctrl-r asks nothing");
    pressed(editing, "<Esc>");
    static_cast<void>(run_ex(editing.controller(), "inoremap jk <Esc>"));
    const std::size_t waiting = asked_count(editing);
    pressed(editing, "aj(");
    expect(asked_count(editing) == waiting + 1 &&
               editing.controller().vim_state().mode == core::VimMode::insert,
           "a ( flushed with a waiting j of inoremap jk asks once");
}

// c-L7: lsp がオフの文書では、Vim の INSERT で `(` を打っても言語サーバーに触れない。
void verify_vim_quiet_when_off()
{
    Editing off(lsp_settings(false));
    off.files().hold_at(std::string(source_path), Bytes{std::string(source_text)});
    applied(off.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    applied(off.controller(), app::SelectEditMode{core::EditMode::vim});
    const std::size_t touches = off.language_server().touches();
    pressed(off, "aadd(1, 2)<Esc>");
    expect(off.language_server().touches() == touches && asked_count(off) == 0,
           "with lsp off typing ( in Vim INSERT touches no language server");
}
} // namespace

void verify_lsp_signature_scope()
{
    verify_reading();
    verify_reading_parameters();
    verify_tip_avoids_completion();
    verify_typing();
    verify_vim_insert();
    verify_quiet_when_off_or_unable();
    verify_operation();
    verify_retrigger_close();
    verify_tip_lifetime();
    verify_tip_scrolled_away();
    verify_tip_in_grid();
    verify_paste_asks_nothing();
    verify_invoked_mark_dropped();
    verify_vim_typed_entry();
    verify_vim_quiet_when_off();
}
} // namespace nenenib::tests
