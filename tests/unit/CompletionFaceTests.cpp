// 契約 --completion-face（Issue #1833・ADR 0190 の決定 1・2・5・9・10・ADR 0169 の決定 3〜5）。
// core の表（候補の並べ方と絞り方・接頭辞の続き・面の配置の上下の切り替え）と、通常モードの補完の
// 面の道（開く → 答えが届く → 絞る → 確定の 1 編集と undo・Esc・閉じる条件・triggerCharacters・
// 候補が無いときの知らせ・要求の前の didChange・面を閉じたら答えを消す）。替え玉
// ScriptedLanguageServer だけを使い、本物のサーバーも窓も使わない。
#include "CaretMotion.hpp"
#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionCandidate.hpp"
#include "CompletionEntries.hpp"
#include "CompletionEntry.hpp"
#include "CompletionFace.hpp"
#include "CompletionFaceLayout.hpp"
#include "CompletionFaceRequest.hpp"
#include "CompletionSource.hpp"
#include "CompletionStep.hpp"
#include "CompletionView.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DeleteUnit.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LanguageServerTick.hpp"
#include "LayoutRect.hpp"
#include "LspCompletionItem.hpp"
#include "LspCompletionList.hpp"
#include "LspPosition.hpp"
#include "LspRange.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "OpenCompletion.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScrollCompletion.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "StepCompletion.hpp"
#include "TestSupport.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VisibleLines.hpp"
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
using core::CompletionEntry;
using core::CompletionSource;
using core::LayoutRect;

constexpr std::string_view source_path = "C:\\work\\main.cpp";
constexpr std::string_view clangd_command = R"("C:\LLVM\bin\clangd.exe" --log=error)";
constexpr std::string_view capable_answer =
    R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","completionProvider":{"triggerCharacters":[".","->"]}}}})";
constexpr std::string_view completion_method = "textDocument/completion";
// 本文の 2 行が frame に載る行数。
constexpr std::size_t shown_lines = 4;

// ------------------------------------------------------------------ core の表

[[nodiscard]] core::LspCompletionItem item(std::string label, std::string sort)
{
    return core::LspCompletionItem{label, std::nullopt, {},          std::move(sort),
                                   label, label,        std::nullopt};
}

[[nodiscard]] std::vector<std::string> labels_of(const std::vector<CompletionEntry> &entries)
{
    std::vector<std::string> labels;
    for (const CompletionEntry &entry : entries)
    {
        labels.push_back(entry.label);
    }
    return labels;
}

// 決定 5: LSP の候補は sortText の順（同じ鍵は届いた順）、本文の語は後ろで、同じ綴りは LSP の 1
// つ。
void verify_entry_order()
{
    const core::LspCompletionList list{
        false, {item("zeta", "b"), item("alpha", "c"), item("mid", "b"), item("first", "a")}};
    const auto server = core::completion_entries_of(list);
    expect(labels_of(server) == std::vector<std::string>{"first", "zeta", "mid", "alpha"},
           "server items follow sortText and keep the arrival order for equal keys");
    expect(server.front().source == CompletionSource::language_server &&
               server.front().filter == "first" && server.front().insert == "first",
           "a server item keeps its filter and insert text and is marked as the language server");
    const std::vector<core::CompletionCandidate> words{{"beta", CompletionSource::buffer},
                                                       {"alpha", CompletionSource::buffer},
                                                       {"gamma", CompletionSource::other_buffers}};
    const auto merged = core::completion_entries(server, words);
    expect(labels_of(merged) ==
               std::vector<std::string>{"first", "zeta", "mid", "alpha", "beta", "gamma"},
           "server items come first and a buffer word with the same spelling is dropped");
    expect(merged.at(4).source == CompletionSource::buffer &&
               merged.at(5).source == CompletionSource::other_buffers &&
               merged.at(4).filter == "beta" && merged.at(4).insert == "beta",
           "buffer words keep their source and insert themselves");
    auto renamed = item("Shown()", "a");
    renamed.insert_text = "beta";
    const auto by_insert =
        core::completion_entries(core::completion_entries_of({false, {renamed}}), words);
    expect(labels_of(by_insert) == std::vector<std::string>{"Shown()", "alpha", "gamma"},
           "a buffer word equal to a server insert text is dropped too");
}

// 決定 5: filter の前方一致・ASCII の大小は区別しない・空の接頭辞は全部。
void verify_matches()
{
    auto filtered = item("Shown", "a");
    filtered.filter_text = "hidden";
    const auto entries = core::completion_entries_of(
        {false, {item("Main", "a"), item("max", "b"), item("amax", "c"), filtered}});
    expect(core::completion_matches(entries, "") == std::vector<std::size_t>{0, 1, 2, 3},
           "an empty prefix keeps every entry");
    expect(core::completion_matches(entries, "ma") == std::vector<std::size_t>{0, 2},
           "the prefix matches the head of the filter text without regard to ASCII case");
    expect(core::completion_matches(entries, "MAX") == std::vector<std::size_t>{2},
           "an upper case prefix matches a lower case entry");
    expect(core::completion_matches(entries, "hid") == std::vector<std::size_t>{1} &&
               core::completion_matches(entries, "sho").empty(),
           "an entry is narrowed by its filter text, not by its label");
    expect(core::completion_matches(entries, "maxx").empty(), "a longer prefix matches nothing");
}

// 決定 5: 打った分が 1 つの接頭辞のままか（語の文字でない文字・別の種類の文字で切れる）。
void verify_prefix_kept()
{
    expect(core::completion_prefix_kept("") && core::completion_prefix_kept("ab_1"),
           "nothing typed and a run of word characters are still one prefix");
    expect(!core::completion_prefix_kept("ab(") && !core::completion_prefix_kept(".") &&
               !core::completion_prefix_kept("a b"),
           "a character that is not a word character ends the prefix");
    expect(core::completion_prefix_kept("\xE6\x97\xA5\xE6\x9C\xAC") &&
               !core::completion_prefix_kept("a\xE6\x97\xA5"),
           "a run of one non-ASCII class is a prefix and a change of class ends it");
    expect(core::completion_source_mark(CompletionSource::buffer) == "." &&
               core::completion_source_mark(CompletionSource::other_buffers) == "b" &&
               core::completion_source_mark(CompletionSource::language_server) == "o",
           "each source has its one-character mark");
}

// ADR 0169 の決定 4: キャレットの下（入らなければ上）・幅 240（補足つきは 360）・最大 8 行。
void verify_face_layout()
{
    const LayoutRect bounds{0, 40, 800, 600};
    const LayoutRect caret{100, 100, 102, 120};
    const auto below = core::completion_face_layout(
        core::CompletionFaceRequest{caret, 100, bounds, 8, 0, false, 96});
    expect(!below.above && below.rows == 8 && below.first == 0 &&
               below.panel == LayoutRect{100, 122, 340, 322},
           "the face opens under the caret, 240 DIP wide and eight rows tall");
    expect(core::completion_face_row(below, 1) == LayoutRect{100, 150, 340, 174} &&
               core::completion_face_mark(below, 0) == LayoutRect{110, 126, 126, 150} &&
               core::completion_face_label(below, 0) == LayoutRect{126, 126, 330, 150} &&
               core::width_of(core::completion_face_detail(below, 0)) == 0,
           "a row holds the source mark and the label, and no detail column");
    const LayoutRect low{100, 500, 102, 520};
    const auto above = core::completion_face_layout(
        core::CompletionFaceRequest{low, 100, bounds, 8, 0, false, 96});
    expect(above.above && above.rows == 8 && above.panel == LayoutRect{100, 298, 340, 498},
           "a caret near the bottom opens the face above it");
    const auto few = core::completion_face_layout(
        core::CompletionFaceRequest{low, 100, bounds, 2, 0, false, 96});
    expect(!few.above && few.rows == 2 && few.panel == LayoutRect{100, 522, 340, 578},
           "a short list still fits under a low caret");
    const auto wide = core::completion_face_layout(
        core::CompletionFaceRequest{caret, 700, bounds, 3, 0, true, 96});
    expect(wide.detailed && wide.panel == LayoutRect{440, 122, 800, 202} &&
               core::completion_face_label(wide, 0) == LayoutRect{466, 126, 670, 150} &&
               core::completion_face_detail(wide, 0) == LayoutRect{680, 126, 790, 150},
           "a detailed face is 360 DIP wide, kept inside the bounds, with the detail on the right");
    const LayoutRect tight{0, 0, 300, 150};
    const LayoutRect middle{20, 60, 22, 80};
    const auto clipped = core::completion_face_layout(
        core::CompletionFaceRequest{middle, 20, tight, 8, 6, false, 96});
    expect(!clipped.above && clipped.rows == 2 && clipped.first == 5 &&
               clipped.panel == LayoutRect{20, 82, 260, 138},
           "a window too short on both sides shows the rows that fit around the chosen one");
    const LayoutRect slit{0, 0, 300, 60};
    const LayoutRect inside{20, 25, 22, 45};
    const auto squeezed = core::completion_face_layout(
        core::CompletionFaceRequest{inside, 20, slit, 8, 3, false, 96});
    expect(squeezed.rows == 1 && squeezed.first == 3 &&
               squeezed.panel == LayoutRect{20, 0, 260, 32},
           "a window without room for one row keeps the face inside the bounds");
    expect(core::completion_face_hit(below, 150, 127) == std::optional<std::size_t>{0} &&
               core::completion_face_hit(below, 150, 317) == std::optional<std::size_t>{7} &&
               !core::completion_face_hit(below, 150, 123).has_value() &&
               !core::completion_face_hit(below, 99, 150).has_value() &&
               core::completion_face_hit(clipped, 30, 90) == std::optional<std::size_t>{5},
           "a point finds the row under it, counted from the first drawn row");
    const auto scaled = core::completion_face_layout(
        core::CompletionFaceRequest{caret, 100, LayoutRect{0, 0, 1600, 1200}, 8, 0, false, 192});
    expect(core::width_of(scaled.panel) == 480 && core::height_of(scaled.panel) == 400,
           "the sizes follow the DPI");
}

// ------------------------------------------------------------------ controller の道

[[nodiscard]] SettingsReading lsp_settings(bool on)
{
    auto settings = core::default_editor_settings();
    settings.lsp = on;
    settings.lsp_commands = {{"cpp", std::string{clangd_command}}};
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

// main.cpp を開き、補完の能力（triggerCharacters は `.` と `->`）つきで `initialize` に答え、
// キャレットを本文の終わり（空の 2 行目）へ置く。
void capable(Editing &editing, std::string_view answer = capable_answer)
{
    editing.files().hold_at(std::string(source_path), Bytes{std::string("int main() {}\n")});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    editing.language_server().serve(app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    editing.language_server().serve(app::ServerMessage{std::string{answer}});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    static_cast<void>(editing.controller().apply(app::VisibleLines{shown_lines}));
    applied(editing.controller(),
            app::MoveCaret{core::CaretMotion::document_end, core::SelectionAnchoring::collapse});
}

[[nodiscard]] app::EditorFrame typed(Editing &editing, std::string text)
{
    return editing.controller().apply_frame(app::InsertText{std::move(text)});
}

[[nodiscard]] app::EditorFrame opened(Editing &editing)
{
    return editing.controller().apply_frame(app::OpenCompletion{});
}

[[nodiscard]] app::EditorFrame stepped(Editing &editing, app::CompletionStep step)
{
    return editing.controller().apply_frame(app::StepCompletion{step});
}

// 最後に送った補完の要求に result で答え、合図の意図で引かせる。
[[nodiscard]] app::EditorFrame answered(Editing &editing, std::string_view result)
{
    auto &server = editing.language_server();
    server.answer(server.requests(completion_method).back(), result);
    return editing.controller().apply_frame(app::WorkCompleted{});
}

// 面の表示値（面が無ければ行の無い値）。
[[nodiscard]] app::CompletionView face_of(const app::EditorFrame &frame)
{
    return frame.completion.value_or(app::CompletionView{0, {}, 0, 0, 0, false});
}

[[nodiscard]] std::vector<std::string> rows(const app::EditorFrame &frame)
{
    std::vector<std::string> labels;
    const app::CompletionView view = face_of(frame);
    for (const app::CompletionRowView &row : view.rows)
    {
        labels.push_back(row.label);
    }
    return labels;
}

// 選んでいる行の名前（面が無ければ "<closed>"）。
[[nodiscard]] std::string chosen(const app::EditorFrame &frame)
{
    if (!frame.completion.has_value())
    {
        return "<closed>";
    }
    const auto &view = face_of(frame);
    return view.rows.at(view.selected).label;
}

[[nodiscard]] std::string message(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

[[nodiscard]] std::string none_notice(Editing &editing)
{
    return std::string(core::ui_text(core::UiMessage::notice_completion_none,
                                     editing.controller().display_language()));
}

[[nodiscard]] std::string text_of(Editing &editing)
{
    return applied(editing.controller(), app::WorkCompleted{});
}

[[nodiscard]] bool contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

// LSP が動いていない文書でも本文の語で開き、打つと絞り、Enter の確定は 1 編集（undo の 1 単位）。
void verify_buffer_words()
{
    Editing editing;
    static_cast<void>(typed(editing, "alpha beta alto al"));
    const auto face = opened(editing);
    expect(rows(face) == std::vector<std::string>{"alpha", "alto"} && chosen(face) == "alpha" &&
               face_of(face).total == 2 && !face_of(face).detailed &&
               face_of(face).rows.front().source == CompletionSource::buffer,
           "Ctrl+Space lists the buffer words for the prefix without a language server");
    expect(editing.controller().completion_active() && editing.language_server().touches() == 0,
           "the face is open and the language server port is not touched");
    const auto narrowed = typed(editing, "t");
    expect(rows(narrowed) == std::vector<std::string>{"alto"} && message(narrowed).empty(),
           "a typed word character goes into the text and narrows the list");
    const auto widened = editing.controller().apply_frame(
        app::DeleteText{core::DeleteDirection::backward, core::DeleteUnit::character});
    expect(rows(widened) == std::vector<std::string>{"alpha", "alto"},
           "Backspace widens the list again");
    static_cast<void>(stepped(editing, app::CompletionStep::next));
    const auto accepted = editing.controller().apply_frame(app::ChooseCompletion{});
    expect(!accepted.completion.has_value() && text_of(editing) == "alpha beta alto alto",
           "Enter replaces the prefix with the chosen word and closes the face");
    applied(editing.controller(), app::HistoryAction{core::HistoryDirection::undo});
    expect(text_of(editing) == "alpha beta alto al", "one undo takes the completion back");
}

// 候補が無ければ開かずに 1 行知らせる。変換中と Vim のモードでは開かない。
void verify_nothing_to_offer()
{
    Editing editing;
    const auto empty = opened(editing);
    expect(!empty.completion.has_value() && message(empty) == none_notice(editing) &&
               !editing.controller().completion_open(),
           "an empty document opens no face and says there are no completions");
    static_cast<void>(typed(editing, "alpha al"));
    const auto composing =
        editing.controller().apply_frame(app::ComposeText{composed_of("\xE3\x81\x82", {}, 1)});
    static_cast<void>(composing);
    const auto refused = opened(editing);
    expect(!refused.completion.has_value() && message(refused).empty(),
           "the face does not open while the IME is composing");
    Editing vim;
    static_cast<void>(typed(vim, "alpha al"));
    applied(vim.controller(), app::SelectEditMode{core::EditMode::vim});
    const auto modal = opened(vim);
    expect(!modal.completion.has_value() && message(modal).empty(),
           "the face does not open in the Vim mode");
}

// 上下は回り込み、PageUp / PageDown は面の行数ぶんで端に止まる。窓は選んだ行を追う。
void verify_steps()
{
    Editing editing;
    std::string words;
    for (std::size_t index = 0; index < 12; ++index)
    {
        words += "w" + std::string(1, static_cast<char>('a' + index)) + "x ";
    }
    static_cast<void>(typed(editing, words + "w"));
    const auto face = opened(editing);
    expect(rows(face).size() == 8 && face_of(face).total == 12 && chosen(face) == "wax",
           "the face shows eight of twelve candidates and chooses the first");
    const auto wrapped = stepped(editing, app::CompletionStep::previous);
    expect(chosen(wrapped) == "wlx" && face_of(wrapped).first == 4,
           "Up from the first row wraps to the last and the window follows");
    const auto paged = stepped(editing, app::CompletionStep::page_previous);
    expect(chosen(paged) == "wdx" && face_of(paged).first == 3, "PageUp moves eight rows up");
    const auto top = stepped(editing, app::CompletionStep::page_previous);
    expect(chosen(top) == "wax" && face_of(top).first == 0, "PageUp stops at the top");
    const auto down = stepped(editing, app::CompletionStep::page_next);
    expect(chosen(down) == "wix" && face_of(down).first == 1, "PageDown moves eight rows down");
    static_cast<void>(stepped(editing, app::CompletionStep::page_next));
    const auto first = stepped(editing, app::CompletionStep::next);
    expect(chosen(first) == "wax" && face_of(first).first == 0,
           "Down from the last row wraps to the first");
}

// 閉じる条件: Esc・語の文字でない文字（その文字は本文に入る）・語の頭より前への Backspace・
// キャレットの移動・タブの切り替え。合図（裏の仕事・言語サーバーの古さ）では閉じない。
void verify_closing()
{
    Editing editing;
    static_cast<void>(typed(editing, "alpha al"));
    static_cast<void>(opened(editing));
    const auto escaped = editing.controller().apply_frame(app::CloseCompletion{});
    expect(!escaped.completion.has_value() && text_of(editing) == "alpha al",
           "Esc closes the face and leaves the text");
    static_cast<void>(opened(editing));
    const auto signalled = editing.controller().apply_frame(app::LanguageServerTick{});
    expect(signalled.completion.has_value() &&
               editing.controller().apply_frame(app::WorkCompleted{}).completion.has_value(),
           "signals keep the face open");
    const auto punctuated = typed(editing, "(");
    expect(!punctuated.completion.has_value() && text_of(editing) == "alpha al(",
           "a character that is not a word character closes the face and still goes in");
    static_cast<void>(typed(editing, " al"));
    static_cast<void>(opened(editing));
    const auto erase =
        app::DeleteText{core::DeleteDirection::backward, core::DeleteUnit::character};
    static_cast<void>(editing.controller().apply_frame(erase));
    const auto at_head = editing.controller().apply_frame(erase);
    expect(at_head.completion.has_value(), "Backspace back to the word start keeps the face");
    const auto before = editing.controller().apply_frame(erase);
    expect(!before.completion.has_value(), "Backspace before the word start closes the face");
    static_cast<void>(typed(editing, " al"));
    static_cast<void>(opened(editing));
    const auto moved = editing.controller().apply_frame(
        app::MoveCaret{core::CaretMotion::previous_character, core::SelectionAnchoring::collapse});
    expect(!moved.completion.has_value(), "moving the caret closes the face");
    static_cast<void>(opened(editing));
    expect(editing.controller().completion_active(),
           "the face opens again in the middle of a word");
    const auto switched = editing.controller().apply_frame(app::NewTab{});
    expect(!switched.completion.has_value() && !editing.controller().completion_open(),
           "switching the tab closes the face");
}

// 決定 1・2・5: 手で開くと要求を送り、届いた LSP
// の候補を本文の語の前に足す。面を閉じたら答えを消す。
void verify_server_candidates()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(typed(editing, "ma"));
    const std::size_t before = server.sent().size();
    const auto face = opened(editing);
    expect(rows(face) == std::vector<std::string>{"main"} &&
               server.requests(completion_method).size() == 1 &&
               server.sent().size() == before + 1 &&
               contains(server.sent().back(), R"("position":{"line":1,"character":2})") &&
               contains(server.sent().back(), R"("context":{"triggerKind":1})"),
           "Ctrl+Space shows the buffer words at once and asks the server at the caret");
    const auto filled = answered(
        editing,
        R"j({"isIncomplete":false,"items":[{"label":"max","sortText":"b"},{"label":"main","sortText":"a","detail":"int ()"},{"label":"zeta"}]})j");
    const auto &view = face_of(filled);
    expect(rows(filled) == std::vector<std::string>{"main", "max"} && chosen(filled) == "main" &&
               view.rows.front().source == CompletionSource::language_server &&
               view.rows.front().detail == "int ()" && view.detailed && view.total == 2,
           "the answer is sorted, narrowed by the prefix and put before the buffer words");
    expect(!editing.controller().language_answers().completion.has_value(),
           "the answer is taken out of the state once the face has it");
    static_cast<void>(typed(editing, "x"));
    expect(server.requests(completion_method).size() == 1,
           "typing narrows a complete answer without asking again");
    static_cast<void>(stepped(editing, app::CompletionStep::next));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    expect(text_of(editing) == "int main() {}|max" && !editing.controller().completion_open(),
           "the chosen server candidate replaces the prefix");
}

// 決定 5: 確定の範囲は textEdit（キャレットの行の中）で、行の外なら接頭辞の置き換えに落とす。
void verify_text_edit_range()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(typed(editing, "x.ma"));
    static_cast<void>(opened(editing));
    static_cast<void>(answered(
        editing,
        R"j([{"label":"mango","textEdit":{"range":{"start":{"line":1,"character":1},"end":{"line":1,"character":4}},"newText":"->mango()"}}])j"));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    expect(text_of(editing) == "int main() {}|x->mango()",
           "a text edit on the caret line replaces from its start to the caret");
    applied(editing.controller(), app::HistoryAction{core::HistoryDirection::undo});
    expect(text_of(editing) == "int main() {}|x.ma", "the text edit is one undo unit");
    static_cast<void>(opened(editing));
    static_cast<void>(answered(
        editing,
        R"([{"label":"mx","filterText":"ma","textEdit":{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":3}},"newText":"mx"}}])"));
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    expect(text_of(editing) == "int main() {}|x.mx",
           "a text edit outside the caret line falls back to replacing the prefix");
}

// 決定 1: 答えが isIncomplete のときだけ、次の打鍵で出し直す。
void verify_incomplete()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(typed(editing, "m"));
    static_cast<void>(opened(editing));
    static_cast<void>(
        answered(editing, R"({"isIncomplete":true,"items":[{"label":"main2"},{"label":"mix"}]})"));
    const auto narrowed = typed(editing, "a");
    expect(server.requests(completion_method).size() == 2 &&
               rows(narrowed) == std::vector<std::string>{"main2", "main"},
           "an incomplete answer is asked again on the next key while the old items are narrowed");
    const auto &sent = server.sent();
    expect(contains(sent.back(), R"("method":"textDocument/completion")") &&
               contains(sent.at(sent.size() - 2), R"("method":"textDocument/didChange")"),
           "the change is sent before the request it belongs to");
    const auto replaced = answered(editing, R"({"isIncomplete":false,"items":[{"label":"mast"}]})");
    expect(rows(replaced) == std::vector<std::string>{"mast", "main"},
           "the newer answer replaces the server items");
    static_cast<void>(typed(editing, "s"));
    expect(server.requests(completion_method).size() == 2, "a complete answer is not asked again");
}

// 決定 1: 面を閉じると待っている要求を取り消し、遅れた答えは使わない。
void verify_cancel_on_close()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(typed(editing, "ma"));
    static_cast<void>(opened(editing));
    const std::int64_t id = server.requests(completion_method).back();
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    expect(server.cancelled() == std::vector<std::int64_t>{id},
           "closing the face cancels the request it was waiting for");
    server.answer(id, R"([{"label":"late"}])");
    const auto late = editing.controller().apply_frame(app::WorkCompleted{});
    expect(!late.completion.has_value() &&
               !editing.controller().language_answers().completion.has_value(),
           "the late answer opens nothing and is not kept");
}

// 決定 5: 答えを待っていて本文の語も無いときは、届いた候補で開く。届いても無ければ知らせる。
void verify_waiting()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(typed(editing, "zz"));
    const auto waiting = opened(editing);
    expect(!waiting.completion.has_value() && message(waiting).empty() &&
               editing.controller().completion_open() && !editing.controller().completion_active(),
           "with no buffer word the face waits for the answer without a notice");
    const auto shown = answered(editing, R"([{"label":"zzz"},{"label":"other"}])");
    expect(rows(shown) == std::vector<std::string>{"zzz"}, "the answer opens the waiting face");
    static_cast<void>(editing.controller().apply_frame(app::CloseCompletion{}));
    static_cast<void>(opened(editing));
    const auto nothing = answered(editing, "[]");
    expect(!nothing.completion.has_value() && message(nothing) == none_notice(editing) &&
               !editing.controller().completion_open(),
           "an empty answer to a waiting face says there are no completions");
}

// 決定 5: 使う人が選ぶ行を動かした後に届いた答えは、選んでいる候補を保つ。
void verify_selection_kept()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(typed(editing, "mile mist mi"));
    static_cast<void>(opened(editing));
    const auto moved = stepped(editing, app::CompletionStep::next);
    expect(chosen(moved) == "mist", "Down chooses the second buffer word");
    const auto filled = answered(editing, R"([{"label":"mix"},{"label":"mint"}])");
    expect(rows(filled) == std::vector<std::string>{"mint", "mix", "mile", "mist"} &&
               chosen(filled) == "mist",
           "the answer goes before the buffer words and the chosen candidate stays chosen");
}

// 決定 5・10: triggerCharacters の 1 文字で開く（LSP が動いている文書だけ）。変更は要求より先に
// 出る。面は LSP の候補が届くまで出さず、候補が無ければ黙って閉じる。自動の面は LSP の候補だけ
// （本文の語を足さない・#1844 の R3 / R4 の決め）。
void verify_trigger_characters()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    auto &server = editing.language_server();
    static_cast<void>(typed(editing, "x"));
    const std::size_t before = server.sent().size();
    const auto dotted = typed(editing, ".");
    const auto &sent = server.sent();
    expect(sent.size() == before + 2 &&
               contains(sent.at(before), R"("method":"textDocument/didChange")") &&
               contains(sent.at(before + 1), R"("method":"textDocument/completion")") &&
               contains(sent.back(), R"("context":{"triggerKind":2,"triggerCharacter":"."})") &&
               contains(sent.back(), R"("position":{"line":1,"character":2})"),
           "a trigger character sends the change first and then the request in the same intent");
    expect(!dotted.completion.has_value() && editing.controller().completion_open(),
           "the automatic face is not shown before the server answers");
    const auto shown = answered(editing, R"([{"label":"size"},{"label":"swap"}])");
    expect(rows(shown) == std::vector<std::string>{"size", "swap"},
           "the answer opens the automatic face with the server items only");
    const auto narrowed = typed(editing, "sw");
    expect(rows(narrowed) == std::vector<std::string>{"swap"}, "typing narrows the automatic face");
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    expect(text_of(editing) == "int main() {}|x.swap", "the choice replaces what was typed");
    static_cast<void>(typed(editing, "-"));
    const std::size_t arrows = server.requests(completion_method).size();
    static_cast<void>(typed(editing, ">"));
    expect(server.requests(completion_method).size() == arrows + 1 &&
               contains(server.sent().back(), R"("triggerCharacter":"->")"),
           "a trigger of two characters opens when its last character is typed");
    const auto silent = answered(editing, "[]");
    expect(!silent.completion.has_value() && message(silent).empty() &&
               !editing.controller().completion_open(),
           "an automatic face with no server item closes without a notice");

    Editing plain(lsp_settings(false));
    plain.files().hold_at(std::string(source_path), Bytes{std::string("int main() {}\n")});
    applied(plain.controller(), app::OpenDocument{core::FilePath::parse(source_path).value()});
    const std::size_t untouched = plain.language_server().touches();
    const auto quiet = typed(plain, ".");
    expect(!quiet.completion.has_value() && !plain.controller().completion_open() &&
               plain.language_server().touches() == untouched,
           "without a running server a trigger character opens nothing and touches no port");
}

// M1（#1844）: trigger
// が語の文字のサーバーでも、語の頭は接頭辞の始まりで、確定は語を二重に入れない。
void verify_word_trigger()
{
    Editing editing(lsp_settings(true));
    capable(
        editing,
        R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"positionEncoding":"utf-8","completionProvider":{"triggerCharacters":["b"]}}}})");
    auto &server = editing.language_server();
    static_cast<void>(typed(editing, "a"));
    static_cast<void>(typed(editing, "b"));
    expect(server.requests(completion_method).size() == 1 &&
               contains(server.sent().back(), R"("triggerCharacter":"b")"),
           "a word character trigger asks the server");
    const auto shown = answered(editing, R"([{"label":"abc"},{"label":"other"}])");
    expect(rows(shown) == std::vector<std::string>{"abc"},
           "the automatic face narrows by the word typed so far, not by nothing");
    static_cast<void>(editing.controller().apply_frame(app::ChooseCompletion{}));
    expect(text_of(editing) == "int main() {}|abc",
           "the choice replaces the word from its start and does not double it");
}

// R5（#1844）: ↑↓ の後に打って絞ったら、その後に届いた答えは先頭を選ぶ。
void verify_step_forgotten()
{
    Editing editing(lsp_settings(true));
    capable(editing);
    static_cast<void>(typed(editing, "mile mist mi"));
    static_cast<void>(opened(editing));
    static_cast<void>(stepped(editing, app::CompletionStep::next));
    const auto narrowed = typed(editing, "s");
    expect(rows(narrowed) == std::vector<std::string>{"mist"} && chosen(narrowed) == "mist",
           "typing narrows the list and chooses its first row");
    const auto filled = answered(editing, R"([{"label":"misty"},{"label":"mis"}])");
    expect(rows(filled) == std::vector<std::string>{"mis", "misty", "mist"} &&
               chosen(filled) == "mis",
           "an answer after narrowing chooses the first server item");
}

// R1（#1844）: 面の行のクリックはその候補を確定し、面の上のホイールは窓だけを送る。
void verify_click_and_wheel()
{
    Editing editing;
    static_cast<void>(typed(editing, "alpha beta alto al"));
    static_cast<void>(opened(editing));
    const auto clicked = editing.controller().apply_frame(app::ChooseCompletion{std::size_t{1}});
    expect(!clicked.completion.has_value() && text_of(editing) == "alpha beta alto alto",
           "a click on a row accepts that candidate");
    static_cast<void>(typed(editing, " al"));
    static_cast<void>(opened(editing));
    const auto outside = editing.controller().apply_frame(app::ChooseCompletion{std::size_t{9}});
    expect(outside.completion.has_value() && text_of(editing) == "alpha beta alto alto al",
           "a row that does not exist accepts nothing");
    Editing many;
    std::string words;
    for (std::size_t index = 0; index < 12; ++index)
    {
        words += "w" + std::string(1, static_cast<char>('a' + index)) + "x ";
    }
    static_cast<void>(typed(many, words + "w"));
    static_cast<void>(opened(many));
    const auto down = many.controller().apply_frame(app::ScrollCompletion{2});
    expect(face_of(down).first == 2 && chosen(down) == "wcx" && text_of(many) == words + "w",
           "the wheel moves the window, keeps the chosen row in it and leaves the text");
    const auto bottom = many.controller().apply_frame(app::ScrollCompletion{40});
    expect(face_of(bottom).first == 4 && chosen(bottom) == "wex", "the window stops at the end");
    const auto up = many.controller().apply_frame(app::ScrollCompletion{-40});
    expect(face_of(up).first == 0 && chosen(up) == "wex",
           "the window stops at the top and a visible chosen row stays");
}

// L1（#1844）: 打った語と同じ綴りの候補だけのとき、確定は本文を変えない（窓は Enter
// を本文へ通す）。
void verify_commits()
{
    Editing editing;
    static_cast<void>(typed(editing, "ab ab"));
    const auto same = opened(editing);
    expect(rows(same) == std::vector<std::string>{"ab"} &&
               !editing.controller().completion_commits(),
           "a face with only the word already typed commits nothing");
    Editing other;
    static_cast<void>(typed(other, "abc ab"));
    static_cast<void>(opened(other));
    expect(other.controller().completion_commits(), "a longer candidate commits");
    static_cast<void>(other.controller().apply_frame(app::CloseCompletion{}));
    expect(!other.controller().completion_commits(), "a closed face commits nothing");
}
} // namespace

void verify_completion_face_contracts()
{
    verify_entry_order();
    verify_matches();
    verify_prefix_kept();
    verify_face_layout();
    verify_buffer_words();
    verify_nothing_to_offer();
    verify_steps();
    verify_closing();
    verify_server_candidates();
    verify_text_edit_range();
    verify_incomplete();
    verify_cancel_on_close();
    verify_waiting();
    verify_selection_kept();
    verify_trigger_characters();
    verify_word_trigger();
    verify_step_forgotten();
    verify_click_and_wheel();
    verify_commits();
}

void verify_completion_face_scope()
{
    verify_completion_face_contracts();
}
} // namespace nenenib::tests
