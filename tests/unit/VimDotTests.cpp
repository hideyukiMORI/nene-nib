// scope `--vim-dot` の単体テスト（ADR 0042 決定 2）。
#include "Editing.hpp"
#include "EditorController.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "VimCharacterExtent.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimLineExtent.hpp"
#include "VimMode.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VimVisualExtent.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::application::SaveDocument;
using nenenib::application::VisibleLines;
using nenenib::core::TextEncoding;
using nenenib::core::VimColumnWish;
using nenenib::core::VimMode;
using nenenib::core::VimState;
using nenenib::core::VirtualColumn;

// 直前の変更が「VISUAL の大きさを持つ記録」かどうか（持っていなければ NORMAL の記録か空）。
[[nodiscard]] bool dot_record_has_extent(const VimState &state)
{
    return state.last_change.has_value() && state.last_change.value().visual.has_value();
}

void verify_vim_dot_recording()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma\nsecond line");
    EditorController &controller = editing.controller();
    vim_replay(controller, "x");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x") &&
               !controller.vim_state().recording.has_value(),
           "a delete is confirmed as the last change and leaves no recording behind");
    vim_replay(controller, "wyyu");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "motion, yank and undo throw their own keys away and keep the last change");
    vim_replay(controller, "d");
    expect(controller.vim_state().recording.has_value(),
           "a pending operator is still being recorded");
    vim_replay(controller, "<Esc>");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "a cancelled operator drops its keys and keeps the previous change");
    vim_replay(controller, "2d3w");
    expect(dot_record_is(controller.vim_state(), nenenib::core::VimCount{6}, "dw"),
           "the operator and motion counts fold into one and no digit is recorded");
    vim_replay(controller, ".");
    expect(dot_record_is(controller.vim_state(), nenenib::core::VimCount{6}, "dw"),
           "the replay records the command again and never records the dot itself");
}

void verify_vim_dot_counts()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij");
    EditorController &controller = editing.controller();
    vim_replay(controller, "x3.");
    expect(vim_body(controller.frame()) == "efghij" &&
               dot_record_is(controller.vim_state(), nenenib::core::VimCount{3}, "x"),
           "a counted dot replaces the count of the recorded change and keeps the new one");
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == "hij",
           "the next dot repeats with the count that was given");
    vim_replay(controller, "2.");
    expect(vim_body(controller.frame()) == "j" &&
               dot_record_is(controller.vim_state(), nenenib::core::VimCount{2}, "x"),
           "and a smaller count replaces it again");
}

void verify_vim_dot_history()
{
    Editing editing;
    open_vim_document(editing, "alpha beta\nsecond line");
    EditorController &controller = editing.controller();
    vim_replay(controller, "3ofoo<Esc>");
    const std::string opened = vim_body(controller.frame());
    const std::string repeated = "alpha beta\nfoo\nfoo\nfoo\nfoo\nfoo\nfoo\nsecond line";
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == repeated,
           "the dot repeats a counted open-line command through the same effects");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == opened, "one dot is one undo unit");
    vim_replay(controller, "<C-r>");
    expect(vim_body(controller.frame()) == repeated, "and redo puts the whole replay back");
    vim_replay(controller, "uu");
    expect(vim_body(controller.frame()) == "alpha beta\nsecond line",
           "the command before the dot is a unit of its own");
}

void verify_vim_dot_document()
{
    Editing editing;
    open_vim_document(editing, "ab\r\ncd");
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    vim_replay(editing.controller(), "ofoo<Esc>.");
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nfoo\r\nfoo\r\ncd",
           "the replayed open-line writes the document's own CRLF bytes");

    Editing narrow;
    open_vim_document(narrow, "one\ntwo\nthree\nfour");
    applied(narrow.controller(), VisibleLines{1});
    vim_replay(narrow.controller(), "dd.");
    expect(narrow.controller().frame().total_lines == 2,
           "the replay passes no window: a one-line viewport repeats the same delete");
}

void verify_vim_dot_modes()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij\nklmnopqrst");
    EditorController &controller = editing.controller();
    vim_replay(controller, "xjvlld");
    expect(dot_record_has_extent(controller.vim_state()),
           "a change finished in VISUAL replaces the last change (ADR 0033 decision 3)");
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == "bcdefghij\nqrst",
           "and the dot after it repeats the VISUAL delete by its extent");

    Editing typing;
    open_vim_document(typing, "abc");
    vim_replay(typing.controller(), "xv<Esc>");
    expect(dot_record_is(typing.controller().vim_state(), std::nullopt, "x"),
           "entering and leaving VISUAL keeps the previous change");
    vim_replay(typing.controller(), "i.<Esc>");
    expect(vim_body(typing.controller().frame()) == ".bc" &&
               dot_record_is(typing.controller().vim_state(), std::nullopt, "i.<Esc>"),
           "in INSERT the dot is an ordinary character and the insert is what repeats");
}

// ---------------------------------------------------------- VISUAL の `.`（Issue #91 / ADR 0033）

// 記録の中身は表示値に出ないので直接見る。VISUAL の記録は回数を持たない（決定 3・実測）。
[[nodiscard]] bool dot_visual_is(const VimState &state,
                                 const nenenib::core::VimVisualExtent &extent,
                                 std::string_view keys)
{
    if (!state.last_change.has_value())
    {
        return false;
    }
    const auto &record = state.last_change.value();
    return record.visual.has_value() && record.visual.value() == extent &&
           record.keys == vim_keys_of(keys) && !record.count.has_value();
}

[[nodiscard]] nenenib::core::VimVisualExtent characters_extent(std::size_t lines,
                                                               std::size_t column)
{
    return nenenib::core::VimCharacterExtent{lines, nenenib::core::VimColumnWish::at_column,
                                             VirtualColumn{column}};
}

// 何を記録するか（決定 3）。変更を起こす鍵から記録が始まり、そのときの選択の大きさが載る。
void verify_vim_visual_dot_records()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij\nklmnopqrst\nuvwxyz0123\nABCDEFGHIJ");
    EditorController &controller = editing.controller();
    vim_replay(controller, "vlld");
    expect(dot_visual_is(controller.vim_state(), characters_extent(1, 3), "d"),
           "a VISUAL delete records the width of the selection and the key that finished it");
    vim_replay(controller, "vlly");
    expect(dot_visual_is(controller.vim_state(), characters_extent(1, 3), "d"),
           "a VISUAL yank changes nothing and keeps the last change");
    vim_replay(controller, "vlloh<Esc>");
    expect(dot_visual_is(controller.vim_state(), characters_extent(1, 3), "d"),
           "moving, swapping the ends and leaving VISUAL record no keys at all");
    vim_replay(controller, "vjld");
    expect(dot_visual_is(controller.vim_state(), characters_extent(2, 2), "d"),
           "several lines record the line count and the last line's own column");
    vim_replay(controller, "v$d");
    expect(dot_visual_is(controller.vim_state(),
                         nenenib::core::VimCharacterExtent{
                             1, nenenib::core::VimColumnWish::at_line_end, VirtualColumn{1}},
                         "d"),
           "a selection made with $ stays to the end of the line instead of freezing a column");
    vim_replay(controller, "Vjd");
    expect(dot_visual_is(controller.vim_state(), nenenib::core::VimLineExtent{2}, "d"),
           "linewise VISUAL records only the number of lines");

    Editing waiting;
    open_vim_document(waiting, "abcd\nefgh\nijkl\nmnop");
    vim_replay(waiting.controller(), "vjlrZ");
    expect(dot_visual_is(waiting.controller().vim_state(), characters_extent(2, 2), "rZ"),
           "r keeps recording until the character it waits for arrives");
    vim_replay(waiting.controller(), "jvlcZZ<Esc>");
    expect(dot_visual_is(waiting.controller().vim_state(), characters_extent(1, 2), "cZZ<Esc>"),
           "c keeps recording the inserted keys until Esc");
    vim_replay(waiting.controller(), "0vlcZ<Home>Y<Esc>");
    expect(!dot_record_has_extent(waiting.controller().vim_state()) &&
               dot_record_is(waiting.controller().vim_state(), std::nullopt, "iY<Esc>"),
           "a move inside the insert retakes the record from i and drops the extent (measured)");
}

// 選び直しと再生（決定 4・5）。期待値は out/issue91-oracle/probe*.txt の固定 Vim 9.1 と同じ。
void verify_vim_visual_dot_replays()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij");
    vim_replay(editing.controller(), "vlld.");
    expect(vim_body(editing.controller().frame()) == "ghij",
           "the dot reselects the same width at the caret and deletes it again");
    vim_replay(editing.controller(), ".");
    expect(vim_body(editing.controller().frame()) == "j", "and the next dot keeps repeating");

    Editing lines;
    open_vim_document(lines, "alpha beta\nsecond line\nthird line\nfourth line");
    vim_replay(lines.controller(), "Vdj.");
    expect(vim_body(lines.controller().frame()) == "second line\nfourth line",
           "linewise repeats the same number of lines at the caret's line");

    Editing across;
    open_vim_document(across, "abcdefghij\nklmnopqrst\nuvwxyz0123\nABCDEFGHIJ");
    vim_replay(across.controller(), "vjldj0.");
    expect(vim_body(across.controller().frame()) == "mnopqrst\nCDEFGHIJ",
           "several lines end at the recorded absolute column of the last line");

    Editing replacing;
    open_vim_document(replacing, "abcdefghij\nklmnopqrst");
    vim_replay(replacing.controller(), "vllrZj0.");
    expect(vim_body(replacing.controller().frame()) == "ZZZdefghij\nZZZnopqrst",
           "the replayed r replaces the same width");

    Editing changing;
    open_vim_document(changing, "abcdefghij\nklmnopqrst");
    vim_replay(changing.controller(), "vllcZZ<Esc>j0.");
    expect(vim_body(changing.controller().frame()) == "ZZdefghij\nZZnopqrst",
           "the replayed c changes the same width and inserts the same text");

    Editing ending;
    open_vim_document(ending, "abc\nklmnopqrst\nuvwxyz0123");
    vim_replay(ending.controller(), "v$d0.");
    expect(vim_body(ending.controller().frame()) == "uvwxyz0123",
           "a $ record takes the whole of a longer line, not the recorded number of columns");
}

// 回数は VISUAL の記録では使わない（決定 6・実測）。大きさが範囲を決める。
void verify_vim_visual_dot_counts()
{
    Editing editing;
    open_vim_document(editing, "abcdefghijklmnopqrst");
    EditorController &controller = editing.controller();
    vim_replay(controller, "vlld2.");
    expect(vim_body(controller.frame()) == "ghijklmnopqrst",
           "a counted dot repeats the recorded extent once, not N times");
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == "jklmnopqrst",
           "and the count is not carried into the next dot either");

    Editing lines;
    open_vim_document(lines, "one\ntwo\nthree\nfour\nfive\nsix\nseven");
    vim_replay(lines.controller(), "Vd2.");
    expect(vim_body(lines.controller().frame()) == "three\nfour\nfive\nsix\nseven",
           "a counted dot on a linewise record still repeats one line");
}

// 範囲が足りないとき（決定 5）と、畳まれても記録は縮まないこと（決定 4・実測）。
void verify_vim_visual_dot_limits()
{
    Editing columns;
    open_vim_document(columns, "abcdefghij\nkl\nmnopqrstuv");
    vim_replay(columns.controller(), "vlldj0.");
    expect(vim_body(columns.controller().frame()) == "defghij\nmnopqrstuv",
           "a line too short for the extent reaches its content end and takes the line break");
    vim_replay(columns.controller(), "j0.");
    expect(vim_body(columns.controller().frame()) == "defghij\npqrstuv",
           "and the clamped replay does not shrink the record for the next dot");

    Editing last;
    open_vim_document(last, "abcdefghij\nkl");
    vim_replay(last.controller(), "vlldj0.");
    expect(vim_body(last.controller().frame()) == "defghij\n",
           "on the last line there is no line break to take");

    Editing document;
    open_vim_document(document, "abcdefghij");
    vim_replay(document.controller(), "vlld$.");
    expect(vim_body(document.controller().frame()) == "defghi",
           "at the end of the document the extent stops at the last character");
    vim_replay(document.controller(), "0.");
    expect(vim_body(document.controller().frame()) == "ghi",
           "and the record still holds the original three columns");

    Editing rows;
    open_vim_document(rows, "abcdefghij\nklmnopqrst\nuvwxyz0123");
    vim_replay(rows.controller(), "vjdG0.");
    expect(vim_body(rows.controller().frame()) == "lmnopqrst\nvwxyz0123",
           "too few lines left folds the extent onto the last line");

    Editing backwards;
    open_vim_document(backwards, "abcdefghij\nklmnopqrst\nuvwxyz0123");
    vim_replay(backwards.controller(), "vjldG4l.");
    expect(vim_body(backwards.controller().frame()) == "mnopqrst\nuz0123",
           "a recorded column to the left of the caret reselects backwards");
}

// NORMAL と VISUAL の記録は入れ替わる（決定 2・3）。undo は `.` 1 回で 1 単位（決定 7）。
void verify_vim_visual_dot_boundaries()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij\nklmnopqrst");
    EditorController &controller = editing.controller();
    vim_replay(controller, "vlldjx.");
    expect(vim_body(controller.frame()) == "defghij\nmnopqrst",
           "a NORMAL change after a VISUAL one takes the record back");

    Editing inside;
    open_vim_document(inside, "abcdefghij\nklmnopqrst");
    vim_replay(inside.controller(), "xjvll.");
    expect(inside.controller().vim_state().mode == nenenib::core::VimMode::visual,
           "the dot does nothing inside VISUAL and does not leave it");
    vim_replay(inside.controller(), "d");
    expect(vim_body(inside.controller().frame()) == "bcdefghij\nnopqrst",
           "so the selection it did not touch is what the operator uses");

    Editing history;
    open_vim_document(history, "abcdefghij");
    vim_replay(history.controller(), "vlld.");
    vim_replay(history.controller(), "u");
    expect(vim_body(history.controller().frame()) == "defghij",
           "one replayed dot is one undo unit");
    vim_replay(history.controller(), "<C-r>");
    expect(vim_body(history.controller().frame()) == "ghij", "and redo puts that unit back");
    vim_replay(history.controller(), "uu");
    expect(vim_body(history.controller().frame()) == "abcdefghij",
           "the VISUAL change before it is a unit of its own");

    Editing crlf;
    open_vim_document(crlf, "abcdefghij\r\nklmnopqrst");
    applied(crlf.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    vim_replay(crlf.controller(), "vlldj0.");
    applied(crlf.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    expect(crlf.files().written() == "defghij\r\nnopqrst",
           "the replayed VISUAL delete keeps the document's own CRLF bytes");
}

// 桁は仮想桁（表示幅）で数える（ADR 0034 の決定 4）。ADR 0033 が残した Tab と幅の混在の穴は
// ここで閉じた。同じ 3 つは virtcol- の fixture にも採ってあり、これは engine 側の説明である。
void verify_vim_visual_dot_columns()
{
    Editing tabs;
    open_vim_document(tabs, "ab\tcd\nxyzwvutsrq");
    vim_replay(tabs.controller(), "vlldj0.");
    expect(vim_body(tabs.controller().frame()) == "cd\nrq",
           "a Tab reaches the next tabstop, so the record covers eight columns");

    Editing kana;
    open_vim_document(kana, "あいうえおかきくけこ\nさしすせそたちつてと");
    vim_replay(kana.controller(), "vlldj0.");
    expect(vim_body(kana.controller().frame()) == "えおかきくけこ\nせそたちつてと",
           "uniform double-width text agrees with Vim because every column is two cells wide");

    Editing mixed;
    open_vim_document(mixed, "abcdefghij\nあいうえおかきくけこ");
    vim_replay(mixed.controller(), "vlldj0.");
    expect(vim_body(mixed.controller().frame()) == "defghij\nうえおかきくけこ",
           "three columns of ASCII reach into the second double-width character");
}

// 取消になった命令は自分の鍵を捨て、直前の変更を変えない（ADR 0030 の決定 3）。固定 Vim では
// ビープが `:normal!` の残りの鍵を捨てるので fixture では測れない。実測は
// out/issue87-oracle/probe4.py が鍵を区切って行い、ここでは同じ規則を直接確かめる。
void verify_vim_dot_cancel(std::string_view before, std::string_view cancelled,
                           std::string_view after)
{
    Editing editing;
    open_vim_document(editing, "ab\ncd\nef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "x");
    vim_replay(controller, before);
    vim_replay(controller, cancelled);
    vim_replay(controller, after);
    vim_replay(controller, ".");
    const std::string note = std::string(cancelled) + " is cancelled and keeps the last change";
    expect(vim_body(controller.frame()) == "b\nd\nef" &&
               dot_record_is(controller.vim_state(), std::nullopt, "x"),
           note.c_str());
}

void verify_vim_dot_cancels()
{
    for (const std::string_view cancelled :
         {"dk", "yk", "3rz", "dfz", "d;", "dq", "dgz", "d<Esc>", "r<Esc>"})
    {
        verify_vim_dot_cancel("", cancelled, "j");
    }
    // 最終行でだけ範囲が作れない命令。先に G で降りてから k で戻る。
    for (const std::string_view cancelled : {"2dd", "2D", "dj"})
    {
        verify_vim_dot_cancel("G", cancelled, "k");
    }
}

void verify_vim_dot_fixtures()
{
    constexpr std::array<std::string_view, 7> boundaries{
        "open-line-count-below", "open-line-count-above", "replace-char-one",  "replace-char-count",
        "char-search-f-count",   "char-search-t-first",   "line-jump-gg-count"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("dot-") || fixture.name.starts_with("visual-dot-") ||
            fixture.name.starts_with("counted-insert-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 181,
           "the scope replays 96 dot and 78 VISUAL dot fixtures and 7 shared boundaries");
}
} // namespace

void verify_vim_dot_contracts()
{
    verify_vim_dot_cancels();
    verify_vim_dot_recording();
    verify_vim_dot_counts();
    verify_vim_dot_history();
    verify_vim_dot_document();
    verify_vim_dot_modes();
    verify_vim_visual_dot_records();
    verify_vim_visual_dot_replays();
    verify_vim_visual_dot_counts();
    verify_vim_visual_dot_limits();
    verify_vim_visual_dot_boundaries();
    verify_vim_visual_dot_columns();
}

void verify_vim_dot_scope()
{
    verify_vim_dot_fixtures();
    verify_vim_dot_contracts();
}
} // namespace nenenib::tests
