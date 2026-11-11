// Issue #1394 / ADR 0009 の追補。通常モードの語の境（Ctrl+← →・Ctrl+Backspace・Ctrl+Delete）。
// 期待値は Windows のメモ帳の engine（Office RichEdit）の実測（記録
// D:/NeNeNib/outputs/20261111/windows-standard-probe.md）。漢字の辞書の切れ方だけは写さず、
// 漢字の連なりを 1 語にする（記録 §5）。
#include "CaretMove.hpp"
#include "DeleteText.hpp"
#include "Editing.hpp"
#include "HistoryAction.hpp"
#include "MoveCaret.hpp"
#include "OpenDocument.hpp"
#include "OrdinaryWordBoundary.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using core::Offset;
using Direction = core::OrdinaryWordDirection;

Offset word_boundary(std::string_view input, std::size_t at, Direction direction)
{
    return core::ordinary_word_boundary(buffer_of(input), Offset{at}, direction);
}

// 本文の頭から Ctrl+→ を押し続けて着いた所に '|' を入れる。
std::string forward_marks(std::string_view input)
{
    const auto text = buffer_of(input);
    std::string marked;
    std::size_t at = 0;
    while (true)
    {
        const std::size_t next =
            core::ordinary_word_boundary(text, Offset{at}, Direction::forward).value;
        if (next <= at)
        {
            break;
        }
        marked.append(input.substr(at, next - at)).append("|");
        at = next;
    }
    return marked.append(input.substr(at));
}

// 本文の末尾から Ctrl+← を押し続けて着いた所に '|' を入れる。
std::string backward_marks(std::string_view input)
{
    const auto text = buffer_of(input);
    std::string marked;
    std::size_t at = input.size();
    while (true)
    {
        const std::size_t previous =
            core::ordinary_word_boundary(text, Offset{at}, Direction::backward).value;
        if (previous >= at)
        {
            break;
        }
        marked.insert(0, std::string("|").append(input.substr(previous, at - previous)));
        at = previous;
    }
    return std::string(input.substr(0, at)).append(marked);
}

// 1 行の本文では、Ctrl+← の止まりは Ctrl+→ の止まりの鏡（記録 §2.1）。
void expect_line_stops(std::string_view input, std::string_view forward, std::string_view backward)
{
    expect(forward_marks(input) == forward, "Ctrl+Right stops at the measured word starts");
    expect(backward_marks(input) == backward, "Ctrl+Left stops at the same word starts");
}

void verify_english_and_symbols()
{
    expect_line_stops("foo bar  baz", "foo |bar  |baz|", "|foo |bar  |baz");
    expect_line_stops("foo.bar(baz) = qux;", "foo|.|bar|(|baz|) |= |qux|;|",
                      "|foo|.|bar|(|baz|) |= |qux|;");
    expect_line_stops("a--b...c  ,d", "a|--|b|...|c  |,|d|", "|a|--|b|...|c  |,|d");
    expect_line_stops("x=1+2; y_z = 3.14", "x|=|1|+|2|; |y|_|z |= |3|.|14|",
                      "|x|=|1|+|2|; |y|_|z |= |3|.|14");
    expect_line_stops("foo_bar1 2baz", "foo|_|bar1 |2baz|", "|foo|_|bar1 |2baz");
    expect_line_stops("it's 'q' a' rock'n'roll", "it's |'|q|' |a|' |rock'n'roll|",
                      "|it's |'|q|' |a|' |rock'n'roll");
    expect_line_stops("a''b it\u2019s", "a|''|b |it\u2019s|", "|a|''|b |it\u2019s");
    expect_line_stops("a@b.com 1,000 #$%", "a|@|b|.|com |1|,|000 |#$%|",
                      "|a|@|b|.|com |1|,|000 |#$%");
    expect_line_stops("a.(b ?! x!\uFF09y", "a|.(|b |?! |x|!\uFF09|y|", "|a|.(|b |?! |x|!\uFF09|y");
}

void verify_blanks_and_tabs()
{
    expect_line_stops("  foo  ", "  |foo  |", "|  |foo  ");
    expect_line_stops("a\tb  \tc", "a|\t|b  |\t|c|", "|a|\t|b  |\t|c");
    expect_line_stops("a\t\tb", "a|\t|\t|b|", "|a|\t|\t|b");
    expect_line_stops("a  \t  b", "a  |\t  |b|", "|a  |\t  |b");
    expect_line_stops("x\u00A0\u00A0y", "x\u00A0\u00A0|y|", "|x\u00A0\u00A0|y");
    expect_line_stops("ab\u3000\u3000cd", "ab\u3000\u3000|cd|", "|ab\u3000\u3000|cd");
}

void verify_japanese()
{
    expect_line_stops("日本語のテキストを編集する。", "日本語|の|テキスト|を|編集|する|。|",
                      "|日本語|の|テキスト|を|編集|する|。");
    expect_line_stops("漢字ひらがなカタカナ", "漢字|ひらがな|カタカナ|", "|漢字|ひらがな|カタカナ");
    expect_line_stops("全角！記号（括弧）「かぎ」、。", "全角|！|記号|（|括弧|）「|かぎ|」、。|",
                      "|全角|！|記号|（|括弧|）「|かぎ|」、。");
    expect_line_stops("Nibで編集するtext、OK? 123円", "Nib|で|編集|する|text|、|OK|? |123|円|",
                      "|Nib|で|編集|する|text|、|OK|? |123|円");
    expect_line_stops("ＡＢＣ１２３ abc", "ＡＢＣ１２３ |abc|", "|ＡＢＣ１２３ |abc");
    expect_line_stops("ＡＢＣabc ２０２６年", "ＡＢＣabc |２０２６|年|", "|ＡＢＣabc |２０２６|年");
    expect_line_stops("ｶﾀｶﾅabc ｱﾞｲ", "ｶﾀｶﾅ|abc |ｱﾞｲ|", "|ｶﾀｶﾅ|abc |ｱﾞｲ");
    expect_line_stops("日本語　全角", "日本語　|全角|", "|日本語　|全角");
    expect_line_stops("アイウ・エオ", "アイウ・エオ|", "|アイウ・エオ");
    // メモ帳は辞書で「飲む」を 1 語にする。Nib は文字の種類で切る（記録 §5・Issue の決めた規則）。
    expect_line_stops("コーヒーを飲む", "コーヒー|を|飲|む|", "|コーヒー|を|飲|む");
}

void verify_combining_characters()
{
    expect_line_stops("cafe\u0301 au lait", "cafe\u0301 |au |lait|", "|cafe\u0301 |au |lait");
    expect_line_stops("abc\u0301!", "abc\u0301|!|", "|abc\u0301|!");
    expect_line_stops("a\u200Db", "a\u200Db|", "|a\u200Db");
    expect_line_stops("か\u3099き", "か\u3099き|", "|か\u3099き");
}

void verify_lines()
{
    // 行末で 1 回止まり、次で改行を越える。空の行の頭にも、字下げの前（行頭）にも止まる。
    expect(forward_marks("foo bar\nbaz qux\n\n  end") == "foo |bar|\n|baz |qux|\n|\n|  |end|",
           "Ctrl+Right stops at each line end and line start");
    // Ctrl+← は字下げの行頭を飛ばし、空の行も越えて前の行末へ着く（メモ帳の実測・記録 §2.3）。
    expect(backward_marks("foo bar\nbaz qux\n\n  end") == "|foo |bar|\n|baz |qux|\n\n  |end",
           "Ctrl+Left skips an indented line start and the empty lines before it");
    constexpr std::array<std::pair<std::string_view, std::array<std::size_t, 3>>, 7> backward{{
        {"ab\n  cd", {5, 2, 0}},   // 字下げの直後から前の行末
        {"ab\n  cd", {3, 2, 0}},   // 行頭からは改行 1 つ
        {"ab\n\n  cd", {6, 2, 0}}, // 空の行も越える
        {"ab\n\n  cd", {4, 3, 0}}, // 行頭からは改行 1 つ（空の行の末尾へ）
        {"ab  \n  cd", {7, 4, 0}}, // 前の行の後ろの空白は越えない
        {"ab\n\tcd", {4, 3, 0}},   // Tab は空白ではないので字下げの規則に入らない
        {"  ab", {2, 0, 0}},       // 先頭の行の字下げは行頭（本文の頭）で止まる
    }};
    for (const auto &[input, step] : backward)
    {
        expect(word_boundary(input, step.at(0), Direction::backward) == Offset{step.at(1)},
               "Ctrl+Left near a line start lands where Notepad lands");
    }
    expect(word_boundary("ab\r\n  cd", 6, Direction::backward) == Offset{2},
           "a CRLF line break is crossed as one unit (indented line)");
    expect(word_boundary("ab\r\ncd", 4, Direction::backward) == Offset{2},
           "Ctrl+Left at a CRLF line start lands before the CR");
    expect(word_boundary("ab\r\ncd", 2, Direction::forward) == Offset{4},
           "Ctrl+Right at a CRLF line end lands after the LF");
    expect(word_boundary("ab", 2, Direction::forward) == Offset{2}, "the end of the text stays");
    expect(word_boundary("ab", 0, Direction::backward) == Offset{0}, "the start of the text stays");
    expect(word_boundary("", 0, Direction::forward) == Offset{0}, "an empty text stays");
}

void verify_caret_move_route()
{
    const auto text = buffer_of("foo.bar baz");
    expect(core::moved_caret(text, Offset{0},
                             core::CaretMoveRequest{core::CaretMotion::next_word, 1,
                                                    core::CaretUnit::ordinary}) == Offset{3},
           "the ordinary unit of moved_caret uses the ordinary word boundary");
    expect(core::moved_caret(text, Offset{0},
                             core::CaretMoveRequest{core::CaretMotion::next_word, 1,
                                                    core::CaretUnit::code_point}) == Offset{8},
           "the code-point unit (Vim mode) keeps the space-only word");
    expect(core::moved_caret(text, Offset{11},
                             core::CaretMoveRequest{core::CaretMotion::previous_word, 1,
                                                    core::CaretUnit::ordinary}) == Offset{8},
           "Ctrl+Left in the ordinary unit lands on the word start");
}

void open_ordinary(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::ordinary});
}

std::string moved(app::EditorController &controller, core::CaretMotion motion,
                  core::SelectionAnchoring anchoring)
{
    return applied(controller, app::MoveCaret{motion, anchoring});
}

void verify_controller_movement()
{
    Editing editing;
    auto &controller = editing.controller();
    open_ordinary(editing, "foo.bar(baz)");
    moved(controller, core::CaretMotion::next_word, core::SelectionAnchoring::collapse);
    expect(controller.frame().caret.position.column == core::Column{4},
           "Ctrl+Right stops before the period");
    moved(controller, core::CaretMotion::next_word, core::SelectionAnchoring::extend);
    moved(controller, core::CaretMotion::next_word, core::SelectionAnchoring::extend);
    expect(controller.frame().caret.position.column == core::Column{8},
           "Ctrl+Shift+Right extends word by word");
    expect(applied(controller, app::DeleteText{core::DeleteDirection::backward,
                                               core::DeleteUnit::word}) == "foo(baz)",
           "Ctrl+Backspace with a selection deletes the selection (Issue #1394)");
}

void verify_controller_deletion()
{
    Editing editing;
    auto &controller = editing.controller();
    open_ordinary(editing, "foo bar  baz\nnext");
    moved(controller, core::CaretMotion::line_end, core::SelectionAnchoring::collapse);
    const app::DeleteText word_back{core::DeleteDirection::backward, core::DeleteUnit::word};
    const app::DeleteText word_forward{core::DeleteDirection::forward, core::DeleteUnit::word};
    expect(applied(controller, word_back) == "foo bar  |next", "Ctrl+Backspace deletes a word");
    expect(applied(controller, word_back) == "foo |next",
           "Ctrl+Backspace deletes a word with its trailing blanks");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}) ==
               "foo bar  |next",
           "undo restores one word deletion");
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    moved(controller, core::CaretMotion::document_start, core::SelectionAnchoring::collapse);
    expect(applied(controller, word_forward) == "bar  baz|next",
           "Ctrl+Delete deletes up to the next word start");
    moved(controller, core::CaretMotion::line_end, core::SelectionAnchoring::collapse);
    expect(applied(controller, word_forward) == "bar  baznext",
           "Ctrl+Delete at a line end deletes the line break");
    expect(applied(controller, word_back) == "bar  next", "Ctrl+Backspace after the join");
    moved(controller, core::CaretMotion::document_start, core::SelectionAnchoring::collapse);
    expect(applied(controller, word_back) == "bar  next",
           "Ctrl+Backspace at the start of the text does nothing");
}

void verify_vim_mode_keeps_words()
{
    Editing editing;
    auto &controller = editing.controller();
    open_ordinary(editing, "foo.bar baz");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    moved(controller, core::CaretMotion::next_word, core::SelectionAnchoring::collapse);
    expect(controller.frame().caret.position.column == core::Column{9},
           "Ctrl+Right in Vim mode keeps the space-only word");
}
} // namespace

void verify_ordinary_word_contracts()
{
    verify_english_and_symbols();
    verify_blanks_and_tabs();
    verify_japanese();
    verify_combining_characters();
    verify_lines();
    verify_caret_move_route();
    verify_controller_movement();
    verify_controller_deletion();
    verify_vim_mode_keeps_words();
}
} // namespace nenenib::tests
