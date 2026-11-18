// 契約 --completion-face の Vim の側（Issue #1845・ADR 0169 の決定 3・ADR 0190 の決定 5）。Vim の
// INSERT の Ctrl-n / Ctrl-p / Ctrl-e / Ctrl-y を、窓が送るのと同じ意図 VimKeyPress
// で流し、補完の面の 行と engine の状態と本文が合うこと・undo の 1 単位・`.`
// の記録・変換中は始まらない・他のタブの語・ 後から届いた LSP
// の候補が列の先頭側に足されて選んでいる語が動かないことを固定する。本文と キャレットとレジスタが
// Vim と合うことは fixture（insert-complete-）が oracle と突き合わせる。 替え玉
// ScriptedLanguageServer だけを使い、本物のサーバーも窓も使わない。
#include "CaretMotion.hpp"
#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionFace.hpp"
#include "CompletionFaceLayout.hpp"
#include "CompletionFaceRequest.hpp"
#include "CompletionSource.hpp"
#include "CompletionView.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "LayoutRect.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "Scopes.hpp"
#include "ScrollCompletion.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "TestSupport.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimInsertCompletion.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

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

constexpr std::string_view words_text = "a1 a2 a1 x a3 a2";
constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view capable_answer =
    R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","completionProvider":{"triggerCharacters":[".","->"]}}}})";
constexpr std::string_view completion_method = "textDocument/completion";
constexpr std::size_t shown_lines = 4;

// 窓と同じに、鍵を 1 つずつ意図 VimKeyPress で送る。最後の frame を返す。
app::EditorFrame pressed(Editing &editing, std::string_view keys)
{
    app::EditorFrame frame = editing.controller().frame();
    for (const core::VimKey &key : vim_keys_of(keys))
    {
        frame = editing.controller().apply_frame(app::VimKeyPress{key});
    }
    return frame;
}

// 面の表示値（面が無ければ行の無い値）。
[[nodiscard]] app::CompletionView face_of(const app::EditorFrame &frame)
{
    return frame.completion.value_or(app::CompletionView{0, {}, 0, 0, 0, false});
}

[[nodiscard]] std::vector<std::string> rows(const app::EditorFrame &frame)
{
    std::vector<std::string> labels;
    if (frame.completion.has_value())
    {
        for (const app::CompletionRowView &row : frame.completion.value().rows)
        {
            labels.push_back(row.label);
        }
    }
    return labels;
}

// 選んでいる行の名前。面が無ければ "<closed>"、どの行も選んでいなければ "<none>"。
[[nodiscard]] std::string chosen(const app::EditorFrame &frame)
{
    if (!frame.completion.has_value())
    {
        return "<closed>";
    }
    const app::CompletionView &view = frame.completion.value();
    return view.selected < view.rows.size() ? view.rows.at(view.selected).label : "<none>";
}

[[nodiscard]] std::string message(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

// 見えている行の本文（意図を流さない: 面を閉じる意図を挟まずに読む）。
[[nodiscard]] std::string body(Editing &editing)
{
    return vim_body(editing.controller().frame());
}

[[nodiscard]] bool completing(Editing &editing)
{
    return editing.controller().vim_state().insert_completion != nullptr;
}

// 面が開く・選ぶたびに本文に入る・輪の端で元の接頭辞へ戻る（どの行も選ばない）・Ctrl-p は逆へ。
void verify_ring()
{
    Editing editing;
    open_vim_document(editing, std::string(words_text));
    const auto first = pressed(editing, "fxcwa<C-n>");
    expect(rows(first) == std::vector<std::string>{"a3", "a2", "a1"} && chosen(first) == "a3" &&
               body(editing) == "a1 a2 a1 a3 a3 a2" && completing(editing) &&
               face_of(first).rows.front().source == core::CompletionSource::buffer,
           "Ctrl-n in INSERT opens the face, chooses the nearest word and puts it into the text");
    const auto second = pressed(editing, "<C-n>");
    expect(chosen(second) == "a2" && body(editing) == "a1 a2 a1 a2 a3 a2" &&
               face_of(second).session == face_of(first).session,
           "the next Ctrl-n chooses the next row of the same face and replaces the word");
    const auto original = pressed(editing, "<C-n><C-n>");
    expect(chosen(original) == "<none>" && rows(original).size() == 3 &&
               body(editing) == "a1 a2 a1 a a3 a2" && completing(editing),
           "past the last word the prefix comes back and no row is chosen while the face stays");
    const auto back = pressed(editing, "<C-p>");
    expect(chosen(back) == "a1" && body(editing) == "a1 a2 a1 a1 a3 a2",
           "Ctrl-p walks the ring the other way");
    expect(editing.language_server().touches() == 0,
           "a document without a language server touches no language server port");
}

// Ctrl-e は元の接頭辞へ戻して閉じ、Ctrl-y は確定して閉じ、文字は確定して入り、Esc は確定して
// NORMAL へ。全部が INSERT の undo の 1 単位。
void verify_closing_keys()
{
    Editing editing;
    open_vim_document(editing, std::string(words_text));
    const auto cancelled = pressed(editing, "fxcwa<C-n><C-n><C-e>");
    expect(!cancelled.completion.has_value() && !completing(editing) &&
               !editing.controller().completion_open() && body(editing) == "a1 a2 a1 a a3 a2",
           "Ctrl-e closes the face and puts the prefix back");
    const auto accepted = pressed(editing, "<C-n><C-y>");
    expect(!accepted.completion.has_value() && !completing(editing) &&
               body(editing) == "a1 a2 a1 a3 a3 a2" &&
               editing.controller().vim_state().mode == core::VimMode::insert,
           "Ctrl-y closes the face, keeps the word and stays in INSERT");
    const auto typed = pressed(editing, "<BS><C-n><C-n>.");
    expect(!typed.completion.has_value() && body(editing) == "a1 a2 a1 a2. a3 a2",
           "a typed character accepts the chosen word and goes in after it");
    const auto escaped = pressed(editing, "<BS><BS><C-n><Esc>");
    expect(!escaped.completion.has_value() && body(editing) == "a1 a2 a1 a3 a3 a2" &&
               editing.controller().vim_state().mode == core::VimMode::normal,
           "Esc accepts the chosen word and leaves INSERT");
    static_cast<void>(pressed(editing, "u"));
    expect(body(editing) == words_text, "the whole insert with its completions is one undo unit");
}

// `.` と回数は確定した語を打った本文として繰り返す（Vim の redo・補完をやり直さない）。
void verify_repeat()
{
    Editing editing;
    open_vim_document(editing, std::string(words_text));
    static_cast<void>(pressed(editing, "fxcwa<C-n><Esc>"));
    expect(dot_record_is(editing.controller().vim_state(), std::nullopt, "cwa3<Esc>"),
           "the dot record holds the completed word as typed text, not the Ctrl-n key");
    static_cast<void>(pressed(editing, "0."));
    expect(body(editing) == "a3 a2 a1 a3 a3 a2",
           "the dot command puts the same word in without completing again");
    Editing counted;
    open_vim_document(counted, std::string(words_text));
    static_cast<void>(pressed(counted, "fx3ia<C-n><Esc>"));
    expect(body(counted) == "a1 a2 a1 a3a3a3x a3 a2",
           "a counted insert repeats the completed word");
}

// 面の行と engine の状態は 1 歩ごとに合う（候補が面の 8 行を超えても窓が選んだ行を追う）。
void verify_face_follows_engine()
{
    Editing editing;
    std::string text;
    for (std::size_t index = 0; index < 12; ++index)
    {
        text += "w" + std::string(1, static_cast<char>('a' + index)) + "x ";
    }
    open_vim_document(editing, text + "w");
    static_cast<void>(pressed(editing, "A"));
    bool follows = true;
    for (std::size_t step = 0; step < 12; ++step)
    {
        const auto frame = pressed(editing, "<C-n>");
        const auto &engine = *editing.controller().vim_state().insert_completion;
        const app::CompletionView view = face_of(frame);
        follows = follows && engine.selected == step && view.total == 12 &&
                  view.rows.size() == core::completion_face_rows &&
                  view.first + view.selected == engine.selected &&
                  chosen(frame) == engine.words->at(engine.selected) &&
                  body(editing) == text + chosen(frame);
    }
    expect(follows, "the chosen row, the engine's position and the text agree at every step");
    const auto ended = pressed(editing, "<C-n>");
    expect(chosen(ended) == "<none>" && body(editing) == text + "w",
           "after the twelfth word the prefix comes back");
}

// 変換中は始まらない。候補が無ければ 1 行知らせて何もしない。engine の外の出来事で面を閉じると
// 選んでいた語は確定し、`.` の記録に残る。
void verify_refusals_and_outside_close()
{
    Editing composing;
    open_vim_document(composing, std::string(words_text));
    static_cast<void>(pressed(composing, "fxcwa"));
    static_cast<void>(
        composing.controller().apply_frame(app::ComposeText{composed_of("\xE3\x81\x82", {}, 1)}));
    const auto refused = pressed(composing, "<C-n>");
    expect(!refused.completion.has_value() && !completing(composing),
           "Ctrl-n does not start a completion while the IME is composing");

    Editing empty;
    open_vim_document(empty, "fo");
    const auto nothing = pressed(empty, "A<C-n>");
    expect(!nothing.completion.has_value() && !completing(empty) && body(empty) == "fo" &&
               message(nothing) == core::ui_text(core::UiMessage::notice_completion_none,
                                                 empty.controller().display_language()),
           "with no candidate Ctrl-n changes nothing and says there are no completions");

    Editing editing;
    open_vim_document(editing, std::string(words_text));
    static_cast<void>(pressed(editing, "fxcwa<C-n>"));
    const auto closed = editing.controller().apply_frame(app::CloseCompletion{});
    expect(!closed.completion.has_value() && !completing(editing) &&
               body(editing) == "a1 a2 a1 a3 a3 a2" &&
               editing.controller().vim_state().mode == core::VimMode::insert,
           "closing the face from outside keeps the chosen word and INSERT");
    static_cast<void>(pressed(editing, "z<Esc>"));
    expect(dot_record_is(editing.controller().vim_state(), std::nullopt, "cwa3z<Esc>"),
           "the word accepted by the outside close is in the dot record");
}

// 面の行のクリックはその候補を入れて確定する（INSERT のまま・`.` の記録に残る）。面のホイールは
// 窓だけを送り、選んでいる語を変えない。
void verify_click()
{
    Editing editing;
    open_vim_document(editing, std::string(words_text));
    static_cast<void>(pressed(editing, "fxcwa<C-n>"));
    const auto scrolled = editing.controller().apply_frame(app::ScrollCompletion{1});
    expect(scrolled.completion.has_value() && completing(editing) &&
               body(editing) == "a1 a2 a1 a3 a3 a2",
           "the wheel on the face keeps the completion and the chosen word");
    const auto clicked = editing.controller().apply_frame(app::ChooseCompletion{2});
    expect(!clicked.completion.has_value() && !completing(editing) &&
               body(editing) == "a1 a2 a1 a1 a3 a2" &&
               editing.controller().vim_state().mode == core::VimMode::insert,
           "a click on a row puts that candidate in, closes the face and stays in INSERT");
    static_cast<void>(pressed(editing, "z<Esc>"));
    expect(dot_record_is(editing.controller().vim_state(), std::nullopt, "cwa1z<Esc>"),
           "the clicked candidate is in the dot record as typed text");
    static_cast<void>(pressed(editing, "u"));
    expect(body(editing) == words_text, "the click is part of the one undo unit of the insert");
}

// 設定の面との排他: 設定の面が開いている間に届いた Ctrl-n は設定の面を閉じるだけで、補完を
// 始めない（その次の Ctrl-n は始める）。
void verify_settings_exclusive()
{
    Editing editing;
    open_vim_document(editing, std::string(words_text));
    static_cast<void>(pressed(editing, "fxcwa"));
    static_cast<void>(editing.controller().apply_frame(app::OpenSettings{}));
    const auto refused = pressed(editing, "<C-n>");
    expect(!refused.completion.has_value() && !completing(editing) &&
               body(editing) == "a1 a2 a1 a a3 a2",
           "Ctrl-n that arrives while the settings face is open starts no completion");
    const auto started = pressed(editing, "<C-n>");
    expect(started.completion.has_value() && completing(editing),
           "the next Ctrl-n starts the completion once the settings face is closed");
}

// 他のタブの語（語の表を持つタブだけ・ADR 0169 の決定 1）は今の文書の語の後ろに並ぶ。
void verify_other_tabs()
{
    Editing editing;
    open_vim_document(editing, "zeta1 zeta2 ze");
    static_cast<void>(pressed(editing, "A<C-n><Esc>"));
    static_cast<void>(editing.controller().apply_frame(app::NewTab{}));
    const auto face = pressed(editing, "ize<C-n>");
    expect(rows(face) == std::vector<std::string>{"zeta1", "zeta2"} && chosen(face) == "zeta1" &&
               face_of(face).rows.front().source == core::CompletionSource::other_buffers &&
               body(editing) == "zeta1",
           "the words of another tab are offered after the words of this document");
}

[[nodiscard]] SettingsReading lsp_settings()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

// main.cpp を開いて補完の能力つきで `initialize` に答え、Vim のモードにする（eol の文書なので
// Vim の本文は 1 行。試験は `o` で 2 行目を開いて打つ）。
void capable(Editing &editing)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string("int main() {}\n")});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    editing.language_server().serve(app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    editing.language_server().serve(app::ServerMessage{std::string{capable_answer}});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::document_end, core::SelectionAnchoring::collapse});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
}

[[nodiscard]] app::EditorFrame answered(Editing &editing, std::string_view result)
{
    auto &server = editing.language_server();
    server.answer(server.requests(completion_method).back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

// 送った補完の要求のどれかが、手で開いた形（triggerKind 1）で position を持つか。
[[nodiscard]] bool asked_at(const std::vector<std::string> &sent, std::string_view position)
{
    for (const std::string &message : sent)
    {
        if (contains(message, completion_method) && contains(message, position) &&
            contains(message, R"("context":{"triggerKind":1})"))
        {
            return true;
        }
    }
    return false;
}

// LSP が動いている文書: 始める鍵で要求を 1 つ送り、後から届いた候補を列の先頭側に足す。選んで
// いる語は動かない（同じ綴りは LSP の 1 つ）。
void verify_server_candidates()
{
    Editing editing(lsp_settings());
    capable(editing);
    auto &server = editing.language_server();
    const auto face = pressed(editing, "omap ma<C-n><C-n>");
    expect(rows(face) == std::vector<std::string>{"main", "map"} && chosen(face) == "map" &&
               server.requests(completion_method).size() == 1 &&
               asked_at(server.sent(), R"("position":{"line":1,"character":6})"),
           "the first Ctrl-n shows the buffer words at once and asks the server once at the caret");
    const auto filled = answered(
        editing,
        R"j([{"label":"max","sortText":"b"},{"label":"main","sortText":"a","detail":"int ()"},{"label":"zeta"}])j");
    const app::CompletionView view = face_of(filled);
    expect(rows(filled) == std::vector<std::string>{"main", "max", "map"} &&
               chosen(filled) == "map" && body(editing) == "int main() {}\nmap map\n" &&
               view.rows.front().source == core::CompletionSource::language_server &&
               view.rows.front().detail == "int ()" && view.detailed &&
               view.rows.back().source == core::CompletionSource::buffer,
           "the answer goes before the buffer words, narrowed by the prefix, and the chosen word "
           "and the text do not move");
    expect(!editing.controller().language_answers().completion.has_value(),
           "the answer is taken out of the state once the face has it");
    const auto stepped = pressed(editing, "<C-p>");
    expect(chosen(stepped) == "max" && body(editing) == "int main() {}\nmap max\n" &&
               server.requests(completion_method).size() == 1,
           "the next key walks the merged list without asking again");
    const auto closed = pressed(editing, "<Esc>");
    expect(!closed.completion.has_value() && body(editing) == "int main() {}\nmap max\n",
           "Esc accepts the server candidate and closes the face");
}

// 本文の語が無くても答えを待ち、届いた候補で面が出る（どの行も選ばない）。空の答えは知らせる。
// 答えを待つ間に閉じたら要求を取り消す。
void verify_waiting()
{
    Editing editing(lsp_settings());
    capable(editing);
    auto &server = editing.language_server();
    const auto waiting = pressed(editing, "oap<C-n>");
    expect(!waiting.completion.has_value() && message(waiting).empty() && completing(editing) &&
               editing.controller().completion_open() && body(editing) == "int main() {}\nap\n",
           "with no buffer word the completion waits for the answer without touching the text");
    // 答えの前の 2 回目の Ctrl-n・Ctrl-p は本文を変えないので、履歴に何も積まず未保存にしない
    // （#1850 の S1）。
    const std::size_t revision = editing.controller().history().revision();
    const auto again = pressed(editing, "<C-n><C-p>");
    expect(!again.completion.has_value() && completing(editing) &&
               editing.controller().history().revision() == revision &&
               body(editing) == "int main() {}\nap\n",
           "stepping a completion that only waits for the answer puts nothing on the history");
    const auto shown = answered(editing, R"([{"label":"Apple"},{"label":"other"}])");
    expect(rows(shown) == std::vector<std::string>{"Apple"} && chosen(shown) == "<none>" &&
               body(editing) == "int main() {}\nap\n",
           "the answer opens the waiting face with no row chosen");
    static_cast<void>(pressed(editing, "<C-n><Esc>0."));
    expect(body(editing) == "int main() {}\nApple\nApple\n",
           "a candidate that does not start with the typed prefix replaces it, and the dot "
           "command repeats the replacement");
    static_cast<void>(pressed(editing, "A zz<C-n>"));
    const auto nothing = answered(editing, "[]");
    expect(!nothing.completion.has_value() && !completing(editing) &&
               message(nothing) == core::ui_text(core::UiMessage::notice_completion_none,
                                                 editing.controller().display_language()),
           "an empty answer to a waiting completion closes it and says there are no completions");
    static_cast<void>(pressed(editing, "<C-n>"));
    const std::size_t waiting_revision = editing.controller().history().revision();
    static_cast<void>(pressed(editing, "<C-e><C-n>"));
    expect(completing(editing) && editing.controller().history().revision() == waiting_revision,
           "Ctrl-e on a waiting completion closes it without touching the history");
    const auto id = server.requests(completion_method).back();
    static_cast<void>(pressed(editing, "x"));
    expect(!server.cancelled().empty() && server.cancelled().back() == id && !completing(editing),
           "a key that ends a waiting completion cancels its request");
}

// `textEdit` の範囲: 語の頭から始まる範囲はそのまま、語の頭より前から始まり入れる本文がその間の
// 本文で始まる範囲は残りを入れ、直せない範囲の候補は列に入れない。
void verify_text_edit_range()
{
    Editing editing(lsp_settings());
    capable(editing);
    static_cast<void>(pressed(editing, "ox.ma<C-n>"));
    const auto filled = answered(
        editing,
        R"j([{"label":"mango","sortText":"a","textEdit":{"range":{"start":{"line":1,"character":2},"end":{"line":1,"character":4}},"newText":"mango()"}},{"label":"mast","sortText":"b","textEdit":{"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":4}},"newText":"x.mast"}},{"label":"mad","sortText":"c","textEdit":{"range":{"start":{"line":1,"character":1},"end":{"line":1,"character":4}},"newText":"->mad"}},{"label":"maze","sortText":"d","textEdit":{"range":{"start":{"line":1,"character":2},"end":{"line":1,"character":6}},"newText":"maze"}}])j");
    expect(rows(filled) == std::vector<std::string>{"mango", "mast", "main"} &&
               chosen(filled) == "main",
           "a candidate whose text edit cannot be written from the word start to the caret is "
           "left out");
    static_cast<void>(pressed(editing, "<C-p>"));
    expect(body(editing) == "int main() {}\nx.mast\n",
           "a text edit that starts before the word start puts in the rest of its text");
    static_cast<void>(pressed(editing, "<C-p>"));
    expect(body(editing) == "int main() {}\nx.mango()\n",
           "a text edit that starts at the word start puts in its whole text");
}

// 面の配置: 選んでいる行が無い（rows 以上）ときは先頭から出す。
void verify_layout_without_choice()
{
    const core::LayoutRect tight{0, 0, 300, 150};
    const core::LayoutRect middle{20, 60, 22, 80};
    const auto layout = core::completion_face_layout(
        core::CompletionFaceRequest{middle, 20, tight, 8, 8, false, 96});
    expect(layout.rows == 2 && layout.first == 0,
           "a face with no chosen row shows its rows from the first");
}
} // namespace

void verify_vim_insert_completion()
{
    verify_ring();
    verify_closing_keys();
    verify_repeat();
    verify_face_follows_engine();
    verify_refusals_and_outside_close();
    verify_click();
    verify_settings_exclusive();
    verify_other_tabs();
    verify_server_candidates();
    verify_waiting();
    verify_text_edit_range();
    verify_layout_without_choice();
}
} // namespace nenenib::tests
