// scope `--vim-change-mark` の単体テスト（#1440・ADR 0181）。Vim の鍵が「undo の単位を作ったか」と
// 「本文を変えたと数えるか」は engine が効果に添える 1 つの印 VimStep.change で、controller は
// 印から空の単位を積み、未保存の判定は本文の位置（EditHistory::content_position）で比べる。
// 1. 契約: fixture 全件の鍵を engine に直に流し、本文を置き換える編集の効果を返した step は印が
//    changed、印の無い step は編集の効果を返さない（効果の後の本文は追わないので、最初の編集か
//    文字組みの問いで止める）。
// 2. engine の印: U だけ（英字の無い `~`・空の行の `x` `C`・`"_p`・空のレジスタの `p`・数の無い
//    `<C-a>`）、U ＋ C で範囲が空（空の行の `>>`）、どちらも無し（空の行の `D`・移動）。
// 3. 未保存の印と undo: saved_only の単位の後は保存済みのまま・空の範囲の changed の後は未保存・
//    `u` で戻ると保存済み・保存の後の saved_only の単位も保存済みのまま・通常モードは空の単位を
//    作らない。
// 4. 通常モードの Ctrl+Z / Ctrl+Y は saved_only の空の単位だけを飛ばし、範囲が空の changed の
//    単位は 1 回で消費する（#1535・追補 9）。Vim の `u` は 1 段ずつ。
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "VimChange.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimMoveTo.hpp"
#include "VimSelect.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorController;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::OpenDocument;
using nenenib::application::SaveDocument;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::collapsed_at;
using nenenib::core::EditMode;
using nenenib::core::HistoryDirection;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::SaveState;
using nenenib::core::Selection;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::vim_replaces_text;
using nenenib::core::vim_step;
using nenenib::core::VimChange;
using nenenib::core::VimEditorView;
using nenenib::core::VimMoveTo;
using nenenib::core::VimSelect;
using nenenib::core::VimState;
using nenenib::core::VimStep;
using nenenib::core::VimViewport;

// 1 つの step が契約を守るか（決定 2）。
[[nodiscard]] bool keeps_contract(const VimStep &step)
{
    const bool replaces = vim_replaces_text(step.effect);
    return (!replaces || step.change == std::optional{VimChange::changed}) &&
           (step.change.has_value() || !replaces);
}

// 歩きを止める step（文字組みの問い・失敗・本文の置き換え）。その後の本文と状態は追わない。
[[nodiscard]] bool stops_walk(const VimStep &step)
{
    return step.layout_request.has_value() || step.row_request.has_value() ||
           step.viewport_request || step.failure.has_value() || vim_replaces_text(step.effect);
}

// 移動と選択の効果だけを追ったキャレット。
[[nodiscard]] Selection selection_after(const VimStep &step, Selection selection)
{
    if (const auto *const moved = std::get_if<VimMoveTo>(&step.effect))
    {
        return collapsed_at(moved->caret);
    }
    if (const auto *const selected = std::get_if<VimSelect>(&step.effect))
    {
        return selected->selection;
    }
    return selection;
}

// fixture の鍵を engine に直に流し、step ごとに契約を確かめる。キャレットは移動と選択の効果だけ
// 追い、最初の編集・失敗・文字組みの問いで止める（その後の本文を追わない）。破った数を返す。
[[nodiscard]] std::size_t broken_steps(const VimFixture &fixture)
{
    const auto text = TextBuffer::from_utf8(fixture.text);
    if (!text.has_value())
    {
        return 0;
    }
    VimState state = empty_vim_state();
    Selection selection = collapsed_at(Offset{0});
    std::size_t broken = 0;
    for (const auto &key : vim_keys_of(fixture.keys))
    {
        const VimEditorView view{text.value(), selection, VimViewport{LineNumber{1}, 64, 0}};
        const VimStep step = vim_step(state, view, key);
        broken += keeps_contract(step) ? 0U : 1U;
        if (stops_walk(step))
        {
            break;
        }
        selection = selection_after(step, selection);
        state = step.next;
    }
    return broken;
}

void verify_fixture_contract()
{
    std::size_t broken = 0;
    for (const VimFixture &fixture : vim_fixtures)
    {
        broken += broken_steps(fixture);
    }
    expect(broken == 0, "every step that replaces text is marked changed and every unmarked "
                        "step leaves the text alone");
}

// 1 行の本文で鍵の列を流した最後の step の印。
[[nodiscard]] std::optional<VimChange> last_mark(std::string_view body, std::string_view keys)
{
    const TextBuffer text = TextBuffer::from_utf8(body).value();
    VimState state = empty_vim_state();
    const Selection selection = collapsed_at(Offset{0});
    std::optional<VimChange> mark;
    for (const auto &key : vim_keys_of(keys))
    {
        const VimStep step =
            vim_step(state, VimEditorView{text, selection, VimViewport{LineNumber{1}, 64, 0}}, key);
        mark = step.change;
        state = step.next;
    }
    return mark;
}

void verify_engine_marks()
{
    const auto saved_only = std::optional{VimChange::saved_only};
    const auto changed = std::optional{VimChange::changed};
    expect(last_mark("123", "~") == saved_only, "~ on digits only saves for undo");
    expect(last_mark("123", "g~~") == saved_only, "g~~ on digits only saves for undo");
    expect(last_mark("abc", "~") == changed, "~ on a letter changes the text");
    expect(last_mark("\nx", "x") == saved_only, "x on an empty line only saves for undo");
    expect(last_mark("abc", "X") == saved_only, "X at the line start only saves for undo");
    expect(last_mark("\nx", "C") == saved_only, "C on an empty line only saves for undo");
    expect(last_mark("\nx", "cc") == changed, "cc on an empty line changes the line");
    expect(last_mark("\nx", ">>") == changed, ">> on an empty line still counts as a change");
    expect(last_mark("abc", "<<") == changed, "<< without indent still counts as a change");
    expect(last_mark("\nx", "D") == std::nullopt, "D on an empty line neither saves nor changes");
    expect(last_mark("abc", "<C-a>") == saved_only, "Ctrl-a without a number only saves");
    expect(last_mark("abc", "\"_p") == saved_only, "\"_p only saves for undo");
    expect(last_mark("abc", "p") == saved_only,
           "p from an unused register is refused after it saves for undo");
    expect(last_mark("abc", "l") == std::nullopt, "a motion carries no mark");
    expect(last_mark("abc", "yl") == std::nullopt, "a yank carries no mark");
}

[[nodiscard]] bool saved(const EditorController &controller)
{
    return controller.documents().at(0).save_state == SaveState::saved;
}

void open_vim(Editing &editing, std::string_view body)
{
    editing.files().hold(Bytes{std::string(body)});
    static_cast<void>(editing.controller().apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(editing.controller().apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(editing.controller().apply_frame(SelectEditMode{EditMode::vim}));
}

void verify_save_state()
{
    Editing digits;
    open_vim(digits, "123\nabc");
    vim_replay(digits.controller(), "~");
    expect(saved(digits.controller()) && digits.controller().history().position() == 1 &&
               digits.controller().history().content_position() == 0,
           "a saved_only unit leaves the document saved");
    vim_replay(digits.controller(), "ju");
    expect(saved(digits.controller()) && digits.controller().history().position() == 0 &&
               digits.controller().frame().caret.position.line == LineNumber{1},
           "u takes back the empty unit and returns to where it was made");

    Editing shifted;
    open_vim(shifted, "\nabc");
    vim_replay(shifted.controller(), ">>");
    expect(!saved(shifted.controller()) && shifted.controller().history().content_position() == 1 &&
               shifted.controller().vim_marks().last_change.has_value(),
           "an empty changed unit marks the document modified and sets '.");
    vim_replay(shifted.controller(), "u");
    expect(saved(shifted.controller()), "u back to the saved text clears the modified mark");
    vim_replay(shifted.controller(), "<C-r>");
    expect(!saved(shifted.controller()), "Ctrl-r brings the change back");

    Editing after_save;
    open_vim(after_save, "abc\n123");
    vim_replay(after_save.controller(), "x");
    static_cast<void>(
        after_save.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    vim_replay(after_save.controller(), "j~");
    expect(saved(after_save.controller()), "a saved_only unit after a save keeps it saved");
    vim_replay(after_save.controller(), "uu");
    expect(!saved(after_save.controller()), "undoing past the save point is modified");

    Editing refused;
    open_vim(refused, "abc");
    vim_replay(refused.controller(), "p");
    expect(saved(refused.controller()) && refused.controller().history().position() == 1 &&
               !refused.controller().vim_marks().last_change.has_value(),
           "a refused p makes one undo unit and leaves the document saved");

    Editing ordinary;
    open_vim(ordinary, "abc");
    static_cast<void>(ordinary.controller().apply_frame(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(ordinary.controller().apply_frame(HistoryAction{HistoryDirection::undo}));
    expect(saved(ordinary.controller()) && ordinary.controller().history().position() == 0,
           "the ordinary mode makes no empty unit");
}

// `C` の後に打つ本文は空の単位に畳まれて 1 回の `u` で戻る（Vim の op_change は 1 単位）。
void verify_change_unit()
{
    Editing editing;
    open_vim(editing, "\nabc");
    vim_replay(editing.controller(), "Cxy<Esc>");
    expect(editing.controller().history().position() == 1 &&
               editing.controller().history().content_position() == 1,
           "typing after C on an empty line folds into the unit C made");
    vim_replay(editing.controller(), "u");
    expect(whole_vim_body(editing.controller()) == "\nabc" && saved(editing.controller()),
           "one u takes back C and the typed text");
}
void press_history(Editing &editing, HistoryDirection direction)
{
    static_cast<void>(editing.controller().apply_frame(HistoryAction{direction}));
}

void select_ordinary(Editing &editing)
{
    static_cast<void>(editing.controller().apply_frame(SelectEditMode{EditMode::ordinary}));
}

// 通常モードの Ctrl+Z / Ctrl+Y は saved_only の空の単位を全部飛ばして次の単位まで 1 回で進み、
// その向きに saved_only の空の単位しか無ければ何もしない（ADR 0181 の追補 9・#1535・D82）。
// Vim の `u` は 1 段ずつのまま。
void verify_ordinary_history_skips_empty_units()
{
    Editing skipped;
    open_vim(skipped, "abc\n123");
    vim_replay(skipped.controller(), "xj~~");
    select_ordinary(skipped);
    press_history(skipped, HistoryDirection::undo);
    expect(skipped.controller().text().text() == "abc\n123" &&
               skipped.controller().history().position() == 0 &&
               skipped.controller().history().content_position() == 0 &&
               saved(skipped.controller()),
           "one ordinary Ctrl+Z skips the empty units and takes back the edit before them");
    press_history(skipped, HistoryDirection::redo);
    expect(skipped.controller().text().text() == "bc\n123" &&
               skipped.controller().history().position() == 1 && !saved(skipped.controller()),
           "one ordinary Ctrl+Y redoes the edit and stops before the empty units");
    press_history(skipped, HistoryDirection::redo);
    expect(skipped.controller().history().position() == 1,
           "ordinary Ctrl+Y with only empty units ahead does nothing");

    Editing alone;
    open_vim(alone, "123");
    vim_replay(alone.controller(), "~");
    select_ordinary(alone);
    press_history(alone, HistoryDirection::undo);
    expect(alone.controller().history().position() == 1 && saved(alone.controller()),
           "ordinary Ctrl+Z with only empty units behind does nothing");

    Editing between;
    open_vim(between, "abc\n123");
    vim_replay(between.controller(), "xj~");
    select_ordinary(between);
    static_cast<void>(between.controller().apply_frame(InsertText{"Z"}));
    press_history(between, HistoryDirection::undo);
    press_history(between, HistoryDirection::undo);
    expect(between.controller().text().text() == "abc\n123" &&
               between.controller().history().position() == 0,
           "two ordinary Ctrl+Z take back the typed text and the edit behind the empty unit");

    Editing vim;
    open_vim(vim, "abc\n123");
    vim_replay(vim.controller(), "xj~u");
    expect(vim.controller().text().text() == "bc\n123" &&
               vim.controller().history().position() == 1,
           "Vim's u still takes back one empty unit at a time");
}

// 範囲が空の changed の単位（空の行の `>>`）は飛ばさず、通常モードの Ctrl+Z / Ctrl+Y 1 回で
// 消費する。本文は変わらないが未保存の印が保存時点へ戻る（V-1541 の M1・追補 9）。
void verify_ordinary_history_takes_empty_changed_units()
{
    Editing shifted;
    open_vim(shifted, "abc\n\n");
    vim_replay(shifted.controller(), "x:w<CR>j>>");
    select_ordinary(shifted);
    press_history(shifted, HistoryDirection::undo);
    expect(shifted.controller().text().text() == "bc\n\n" &&
               shifted.controller().history().position() == 1 &&
               shifted.controller().history().content_position() == 1 &&
               saved(shifted.controller()),
           "one ordinary Ctrl+Z takes back the empty >> and returns to the saved text");
    press_history(shifted, HistoryDirection::undo);
    expect(shifted.controller().text().text() == "abc\n\n" &&
               shifted.controller().history().position() == 0 && !saved(shifted.controller()),
           "the second ordinary Ctrl+Z takes back the x before the save");
    press_history(shifted, HistoryDirection::redo);
    press_history(shifted, HistoryDirection::redo);
    expect(shifted.controller().text().text() == "bc\n\n" &&
               shifted.controller().history().position() == 2 && !saved(shifted.controller()),
           "two ordinary Ctrl+Y redo the x and the empty >>");

    Editing mixed;
    open_vim(mixed, "abc\n\n123");
    vim_replay(mixed.controller(), "xjj~k>>");
    select_ordinary(mixed);
    press_history(mixed, HistoryDirection::undo);
    expect(mixed.controller().history().position() == 2 &&
               mixed.controller().history().content_position() == 1,
           "ordinary Ctrl+Z takes the empty >> alone and leaves the ~ behind it");
    press_history(mixed, HistoryDirection::undo);
    expect(mixed.controller().text().text() == "abc\n\n123" &&
               mixed.controller().history().position() == 0 && saved(mixed.controller()),
           "the next ordinary Ctrl+Z skips the ~ and takes back the x");
    press_history(mixed, HistoryDirection::redo);
    expect(mixed.controller().history().position() == 1,
           "ordinary Ctrl+Y redoes the x and stops before the ~");
    press_history(mixed, HistoryDirection::redo);
    expect(mixed.controller().history().position() == 3 &&
               mixed.controller().history().content_position() == 2 && !saved(mixed.controller()),
           "the next ordinary Ctrl+Y skips the ~ and redoes the empty >>");
}

// `~` の後に通常モードで打った本文を Ctrl+Z ×2 → Ctrl+Y ×2 で往復する（V-1541 の (c)）。
void verify_ordinary_history_round_trip()
{
    Editing typed;
    open_vim(typed, "123");
    vim_replay(typed.controller(), "~");
    select_ordinary(typed);
    static_cast<void>(typed.controller().apply_frame(InsertText{"Z"}));
    const std::string after_typing = typed.controller().text().text();
    press_history(typed, HistoryDirection::undo);
    expect(typed.controller().text().text() == "123" &&
               typed.controller().history().position() == 1 && saved(typed.controller()),
           "ordinary Ctrl+Z takes back the typed text and keeps the ~");
    press_history(typed, HistoryDirection::undo);
    expect(typed.controller().history().position() == 1 && saved(typed.controller()),
           "ordinary Ctrl+Z with only the ~ behind does nothing");
    press_history(typed, HistoryDirection::redo);
    expect(typed.controller().text().text() == after_typing &&
               typed.controller().history().position() == 2 && !saved(typed.controller()),
           "ordinary Ctrl+Y redoes the typed text");
    press_history(typed, HistoryDirection::redo);
    expect(typed.controller().history().position() == 2, "ordinary Ctrl+Y at the end does nothing");
}

// saved_only の単位だけが並ぶ端: どちらの向きも何もしない（undo の側と redo の側）。
void verify_ordinary_history_saved_only_edges()
{
    Editing behind;
    open_vim(behind, "123\n456");
    vim_replay(behind.controller(), "~j~");
    select_ordinary(behind);
    const std::size_t revision = behind.controller().history().revision();
    press_history(behind, HistoryDirection::undo);
    expect(behind.controller().history().position() == 2 &&
               behind.controller().history().revision() == revision && saved(behind.controller()),
           "ordinary Ctrl+Z with only saved_only units behind moves nothing");

    Editing ahead;
    open_vim(ahead, "123\n456");
    vim_replay(ahead.controller(), "~j~uu");
    select_ordinary(ahead);
    press_history(ahead, HistoryDirection::redo);
    expect(ahead.controller().history().position() == 0 && saved(ahead.controller()),
           "ordinary Ctrl+Y with only saved_only units ahead moves nothing");
    press_history(ahead, HistoryDirection::undo);
    expect(ahead.controller().history().position() == 0,
           "ordinary Ctrl+Z at the start does nothing");
}

// EditHistory の口そのもの: saved_only の空の単位だけを飛ばし、飛ばす間に本文の位置は変わらない。
// changed の空の単位と本文を変える単位の手前で止まる。
void verify_history_skips_only_saved_only_units()
{
    using nenenib::core::EditBoundary;
    using nenenib::core::EditHistory;
    const nenenib::core::Edit empty{Offset{0}, "", "", Offset{0}};
    const nenenib::core::Edit typed{Offset{0}, "", "a", Offset{0}};
    const auto history = EditHistory::empty()
                             .pushed(typed, EditBoundary::separate)
                             .pushed(empty, EditBoundary::separate, VimChange::saved_only)
                             .pushed(empty, EditBoundary::separate, VimChange::changed)
                             .pushed(empty, EditBoundary::separate, VimChange::saved_only);
    const auto first = history.past_saved_only_units(HistoryDirection::undo);
    expect(first.has_value() && first.value().position() == 3 &&
               first.value().content_position() == history.content_position() &&
               first.value().revision() == history.revision() + 1,
           "the walk skips one saved_only unit and stops before the empty changed unit");
    const auto stays = first.value().past_saved_only_units(HistoryDirection::undo);
    expect(stays.has_value() && stays.value().position() == 3 &&
               stays.value().revision() == first.value().revision(),
           "an empty changed unit next is not skipped and nothing moves");
    const auto behind = first.value().undone().past_saved_only_units(HistoryDirection::undo);
    expect(behind.has_value() && behind.value().position() == 1 &&
               behind.value().content_position() == 1,
           "the walk skips the saved_only unit before the typed unit and keeps the content "
           "position");
    const auto only = EditHistory::empty()
                          .pushed(empty, EditBoundary::separate, VimChange::saved_only)
                          .pushed(empty, EditBoundary::separate, VimChange::saved_only);
    expect(!only.past_saved_only_units(HistoryDirection::undo).has_value() &&
               !only.past_saved_only_units(HistoryDirection::redo).has_value(),
           "only saved_only units in a direction are the end of the history");
}
} // namespace

void verify_vim_change_mark_scope()
{
    verify_fixture_contract();
    verify_engine_marks();
    verify_save_state();
    verify_change_unit();
    verify_ordinary_history_skips_empty_units();
    verify_ordinary_history_takes_empty_changed_units();
    verify_ordinary_history_round_trip();
    verify_ordinary_history_saved_only_edges();
    verify_history_skips_only_saved_only_units();
}
} // namespace nenenib::tests
