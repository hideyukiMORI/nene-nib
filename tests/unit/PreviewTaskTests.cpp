// プレビューのタスクリストの切り替えの application の契約（ADR 0155 の追補 24〜26・#1101）。
// クリックの意図 → 印の 1 バイトを置き換える Edit 1 回・undo で戻る・古い結果の 1
// 行・改訂番号の道で 描き直す・キャレットは動かない。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "PreviewTick.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedMarkdown.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TogglePreview.hpp"
#include "TogglePreviewTask.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;

// 見えている行を改行でつないだ本文（試験の本文は枠に収まる）。
[[nodiscard]] std::string body_of(const EditorFrame &frame)
{
    std::string joined;
    for (const auto &line : frame.lines)
    {
        if (!joined.empty())
        {
            joined += "\n";
        }
        joined += line.text;
    }
    return joined;
}

[[nodiscard]] std::string message_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

// 最後の要求の本文を本物の解析器で読んで、その券の結果を置く。
void serve_parsed(Editing &editing)
{
    const auto &request = editing.markdown().requests().back();
    auto parsed = core::parse_markdown(*request.text, core::MarkdownStop::never());
    editing.markdown().serve(app::MarkdownOutcome{request.ticket, std::move(parsed)});
    applied(editing.controller(), app::WorkCompleted{});
}

// 保存済みのファイルを開いてプレビューを置き、最初の券の結果を見せる。
void open_previewed(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::TogglePreview{});
    serve_parsed(editing);
}

constexpr std::string_view tasks = "- [ ] a\n- [x] b\n- [X] c";

// 1. クリックは印の 1 バイトを置き換える Edit 1 回で、undo 1 回で戻る（追補 24）。
void verify_click_toggles_one_byte()
{
    Editing editing;
    auto &controller = editing.controller();
    open_previewed(editing, std::string(tasks));
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{2}, core::Column{3}},
                                        core::SelectionAnchoring::collapse});
    const EditorFrame checked = controller.apply_frame(app::TogglePreviewTask{0});
    expect(body_of(checked) == "- [x] a\n- [x] b\n- [X] c",
           "clicking an unchecked task writes x into its mark");
    expect(checked.document.save_state == core::SaveState::modified,
           "the toggle is an edit and marks the document unsaved");
    expect(checked.caret.position.line.value == 2 && checked.caret.position.column.value == 3,
           "the caret of the body stays where it was");
    const EditorFrame unchecked = controller.apply_frame(app::TogglePreviewTask{1});
    expect(body_of(unchecked) == "- [x] a\n- [ ] b\n- [X] c",
           "clicking a checked task writes a space into its mark");
    const EditorFrame upper = controller.apply_frame(app::TogglePreviewTask{2});
    expect(body_of(upper) == "- [x] a\n- [ ] b\n- [ ] c", "an upper case X is checked too");
    const EditorFrame undone =
        controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(body_of(undone) == "- [x] a\n- [ ] b\n- [X] c", "one undo takes back one toggle");
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    const EditorFrame original =
        controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(body_of(original) == tasks && original.document.save_state == core::SaveState::saved,
           "three undos give back the saved body");
}

// 2. 解析の後に本文が変わり、印の位置が `[ ]` の形でなければ何もせず 1 行知らせる（追補 24）。
void verify_stale_result_is_refused()
{
    Editing editing;
    auto &controller = editing.controller();
    open_previewed(editing, std::string(tasks));
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{0}, core::Column{0}},
                                        core::SelectionAnchoring::collapse});
    applied(controller, app::InsertText{"z"});
    const EditorFrame refused = controller.apply_frame(app::TogglePreviewTask{0});
    expect(body_of(refused) == "z- [ ] a\n- [x] b\n- [X] c",
           "a mark that moved since the parse is not touched");
    expect(message_of(refused) == "プレビューが古いので切り替えられません",
           "a stale result says so in one line");
    const EditorFrame next = controller.apply_frame(app::InsertText{"y"});
    expect(message_of(next).empty(), "the line lasts for one intent");
    const EditorFrame plain = controller.apply_frame(app::TogglePreviewTask{9});
    expect(body_of(plain) == "zy- [ ] a\n- [x] b\n- [X] c" &&
               message_of(plain) == "プレビューが古いので切り替えられません",
           "a block out of the result is refused with the same line");

    Editing paragraph;
    open_previewed(paragraph, "text\n\n- [ ] a");
    const EditorFrame no_task = paragraph.controller().apply_frame(app::TogglePreviewTask{0});
    expect(body_of(no_task) == "text\n\n- [ ] a" &&
               message_of(no_task) == "プレビューが古いので切り替えられません",
           "a block without a task is refused");
}

// 3. 描き直しは改訂番号の道（決定 8）。切り替えの後に tick 2 回で今の本文を頼む。
void verify_redraw_through_revision()
{
    Editing editing;
    auto &controller = editing.controller();
    open_previewed(editing, std::string(tasks));
    const auto &markdown = editing.markdown();
    applied(controller, app::TogglePreviewTask{0});
    expect(markdown.requests().size() == 1, "the toggle itself requests nothing");
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    expect(markdown.requests().size() == 2 &&
               *markdown.requests().back().text == "- [x] a\n- [x] b\n- [X] c",
           "the ticks after the toggle request the toggled body");
    // 新しい結果が来る前の 2 回目のクリックは、今の本文の印を読んで戻す。
    const EditorFrame again = controller.apply_frame(app::TogglePreviewTask{0});
    expect(body_of(again) == tasks, "a second click before the new result reads the body");
}

// 4. Vim の NORMAL でも同じ Edit 1 回で、`u` で戻り、キャレットは動かない。
void verify_vim_toggle()
{
    Editing editing;
    auto &controller = editing.controller();
    open_previewed(editing, std::string(tasks));
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_replay(controller, "l");
    const auto before = controller.frame().caret.position;
    const EditorFrame toggled = controller.apply_frame(app::TogglePreviewTask{2});
    expect(body_of(toggled) == "- [ ] a\n- [x] b\n- [ ] c" && toggled.caret.position == before,
           "in Vim the toggle edits the mark and keeps the caret");
    vim_replay(controller, "u");
    expect(body_of(controller.frame()) == tasks, "u takes back the toggle");
}

// 5. CRLF の文書でも印の位置は解析に渡した本文のバイトで合う。
void verify_crlf_document()
{
    Editing editing;
    auto &controller = editing.controller();
    open_previewed(editing, "- [ ] a\r\n- [x] b");
    const EditorFrame toggled = controller.apply_frame(app::TogglePreviewTask{1});
    expect(body_of(toggled) == "- [ ] a\n- [ ] b" &&
               toggled.document.save_state == core::SaveState::modified,
           "a CRLF document toggles the mark of the second line");
}
} // namespace

void verify_preview_task_scope()
{
    verify_click_toggles_one_byte();
    verify_stale_result_is_refused();
    verify_redraw_through_revision();
    verify_vim_toggle();
    verify_crlf_document();
}
} // namespace nenenib::tests
