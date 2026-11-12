// 本文を続けて押した 2 回目と 3 回目（#1250・ADR 0056 の決定 9 の追補）。替え玉の文字組みは 1 升 10
// 画素・全角 2 升・幅 0 の結合文字は 0 升で、x は本文の左端からの画素。通常モードは Ctrl+C と同じ
// 写しで選択を読み、Vim は VISUAL の種類と `y` の無名レジスタで読む。
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "PlaceCaret.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimMode.hpp"
#include "VimNamedRegisters.hpp"
#include "VimRegisterKind.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using namespace nenenib::core;
using namespace nenenib::application;

// 1 行目は語と空白の連なり、2 行目は空行、3 行目は全角、4 行目は結合文字（e + U+0301 で 1 文字）。
constexpr std::string_view sample = "foo bar  baz\n\n\xE6\x97\xA5\xE6\x9C\xAC "
                                    "\xE3\x81\x8B\xE3\x81\xAA\ncafe\xCC\x81 x";

void prepare(EditorController &controller)
{
    static_cast<void>(controller.apply(InsertText{std::string(sample)}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(VisibleLines{5, TextViewGeometry{400, 20, 96}}));
}

void press(EditorController &controller, float x, std::int64_t row, PointerClicks clicks)
{
    static_cast<void>(controller.apply(PointText{x, row, SelectionAnchoring::collapse, clicks}));
}

[[nodiscard]] std::string copied(Editing &editing)
{
    static_cast<void>(editing.controller().apply(ClipboardAction{ClipboardOperation::copy}));
    return editing.clipboard().read().value_or("");
}

[[nodiscard]] std::string ordinary_selected(float x, std::int64_t row, PointerClicks clicks)
{
    Editing editing;
    prepare(editing.controller());
    press(editing.controller(), x, row, clicks);
    return copied(editing);
}

void verify_ordinary_word()
{
    expect(ordinary_selected(15, 0, PointerClicks::double_) == "foo",
           "a double click on a letter selects the word like Vim's iw");
    expect(ordinary_selected(28, 0, PointerClicks::double_) == "foo",
           "the right half of the last letter still points at that letter, not the space after");
    expect(ordinary_selected(75, 0, PointerClicks::double_) == "  ",
           "a double click on blanks selects the run of blanks like iw");
    expect(ordinary_selected(300, 0, PointerClicks::double_) == "baz",
           "a double click past the line end selects the last word of the line");
    expect(ordinary_selected(5, 1, PointerClicks::double_) == "\n",
           "a double click on an empty line selects its newline, not an empty selection");
    expect(ordinary_selected(25, 2, PointerClicks::double_) == "\xE6\x97\xA5\xE6\x9C\xAC",
           "a double click on a full-width character selects the full-width word");
    expect(ordinary_selected(38, 2, PointerClicks::double_) == "\xE6\x97\xA5\xE6\x9C\xAC",
           "the right half of a full-width character points at that character");
    expect(ordinary_selected(38, 3, PointerClicks::double_) == "cafe\xCC\x81",
           "a combining accent belongs to the character before it");
    expect(ordinary_selected(15, 0, PointerClicks::single).empty(),
           "a single press still places the caret without a selection");
}

void verify_ordinary_line()
{
    expect(ordinary_selected(15, 0, PointerClicks::triple) == "foo bar  baz\n",
           "a triple click selects the line with its newline");
    expect(ordinary_selected(5, 1, PointerClicks::triple) == "\n",
           "a triple click on an empty line selects its newline");
    expect(ordinary_selected(15, 3, PointerClicks::triple) == "cafe\xCC\x81 x",
           "a triple click on the last line selects up to the end of the text");
}

void prepare_vim(EditorController &controller)
{
    prepare(controller);
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
}

[[nodiscard]] bool yanked(EditorController &controller, std::string_view text, VimRegisterKind kind)
{
    vim_replay(controller, "y");
    const auto &unnamed = controller.vim_state().unnamed_register.value();
    return unnamed.text == text && unnamed.kind == kind &&
           controller.frame().vim_mode == VimMode::normal;
}

void verify_vim_word()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    press(controller, 45, 0, PointerClicks::double_);
    expect(controller.frame().vim_mode == VimMode::visual,
           "a double click in Vim enters characterwise VISUAL");
    expect(yanked(controller, "bar", VimRegisterKind::characters),
           "the VISUAL selection is the word under the pointer");
    vim_replay(controller, "gv");
    expect(controller.frame().vim_mode == VimMode::visual &&
               yanked(controller, "bar", VimRegisterKind::characters),
           "gv reselects the word chosen by the double click");
    press(controller, 5, 1, PointerClicks::double_);
    expect(yanked(controller, "\n", VimRegisterKind::characters),
           "a double click on an empty line selects the newline like viw");
    press(controller, 38, 3, PointerClicks::double_);
    expect(yanked(controller, "cafe\xCC\x81", VimRegisterKind::characters),
           "a combining accent belongs to the word in Vim too");
}

// 改行で終わる本文 `foo\n` の 2 行目（空の最後の行・#1507）。`iw` は前の行の末尾へ戻るが、押しは
// 空行に留まる（gvim の find_start_of_word・メモ帳も何も選ばない）。
void prepare_last_empty(EditorController &controller)
{
    static_cast<void>(controller.apply(InsertText{"foo\n"}));
    static_cast<void>(controller.apply(VisibleLines{5, TextViewGeometry{400, 20, 96}}));
}

void verify_ordinary_last_empty_line()
{
    Editing editing;
    prepare_last_empty(editing.controller());
    press(editing.controller(), 5, 1, PointerClicks::double_);
    expect(copied(editing).empty(),
           "a double click on the empty last line selects nothing, not the previous line's end");
    expect(editing.controller().frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "and the caret stays on the empty last line");
}

void verify_vim_last_empty_line()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_last_empty(controller);
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    press(controller, 5, 1, PointerClicks::double_);
    expect(controller.frame().vim_mode == VimMode::visual &&
               controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "a double click on the empty last line in Vim stays in VISUAL on that line");
    expect(yanked(controller, "", VimRegisterKind::characters),
           "the VISUAL selection does not reach back into the previous line");
}

void verify_vim_line()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    press(controller, 15, 0, PointerClicks::triple);
    expect(controller.frame().vim_mode == VimMode::visual_line,
           "a triple click in Vim enters linewise VISUAL");
    expect(yanked(controller, "foo bar  baz\n", VimRegisterKind::lines),
           "the linewise VISUAL covers the pointed line");
}

void verify_vim_from_visual()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    vim_replay(controller, "vl");
    press(controller, 95, 0, PointerClicks::double_);
    expect(controller.frame().vim_mode == VimMode::visual,
           "a double click inside VISUAL starts a new characterwise VISUAL");
    vim_replay(controller, "<Esc>");
    vim_replay(controller, "gv");
    expect(yanked(controller, "baz", VimRegisterKind::characters),
           "gv after leaving reselects the clicked word, not the earlier selection");
    vim_replay(controller, "i");
    press(controller, 15, 0, PointerClicks::double_);
    expect(controller.frame().vim_mode == VimMode::visual &&
               yanked(controller, "foo", VimRegisterKind::characters),
           "a double click in INSERT leaves INSERT and selects the word");
}
// 押しは鍵ではない（V-1255 の R1）。`.` の記録・待ち・INSERT の回数・マクロの録画を鍵の意味で
// 通らない。
void verify_vim_keeps_repeat()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    vim_replay(controller, "x");
    press(controller, 35, 0, PointerClicks::double_);
    vim_replay(controller, "<Esc>");
    vim_replay(controller, ".");
    expect(whole_vim_body(controller).starts_with("oo ba  baz\n"),
           "a double click does not forget the change that . repeats");
}

void verify_vim_waits_dropped()
{
    for (const std::string_view keys : {"vi", "i<C-r>"})
    {
        Editing editing;
        auto &controller = editing.controller();
        prepare_vim(controller);
        vim_replay(controller, keys);
        press(controller, 45, 0, PointerClicks::double_);
        expect(whole_vim_body(controller) == sample &&
                   controller.frame().vim_mode == VimMode::visual,
               "a double click in the middle of a waiting key types nothing and enters VISUAL");
        expect(yanked(controller, "bar", VimRegisterKind::characters),
               "the word is selected after the wait is dropped");
    }
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    vim_replay(controller, "i<C-r>");
    press(controller, 15, 0, PointerClicks::triple);
    expect(whole_vim_body(controller) == sample &&
               controller.frame().vim_mode == VimMode::visual_line,
           "a triple click in the middle of a waiting key enters linewise VISUAL");
}

void verify_vim_insert_count_dropped()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    vim_replay(controller, "3ifoo");
    press(controller, 95, 0, PointerClicks::double_);
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller).starts_with("foofoo bar  baz\n"),
           "a double click leaves a counted INSERT without repeating it");
}

void verify_vim_macro_untouched()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_vim(controller);
    vim_replay(controller, "qa");
    press(controller, 45, 0, PointerClicks::double_);
    vim_replay(controller, "yq");
    const auto index = vim_register_index(U'a').value_or(0);
    const auto &macro = controller.vim_state().registers.registers.at(index);
    expect(macro.value().text == "y", "a double click while recording a macro is not recorded");
}
} // namespace

void verify_pointer_scope()
{
    verify_ordinary_word();
    verify_ordinary_line();
    verify_vim_word();
    verify_vim_line();
    verify_ordinary_last_empty_line();
    verify_vim_last_empty_line();
    verify_vim_from_visual();
    verify_vim_keeps_repeat();
    verify_vim_waits_dropped();
    verify_vim_insert_count_dropped();
    verify_vim_macro_untouched();
}
} // namespace nenenib::tests
