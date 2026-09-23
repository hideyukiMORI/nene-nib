// scope `--vim-visual-wanted` の単体テスト（ADR 0042 決定 2）。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VimCharacter.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimSelect.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
using nenenib::application::OpenDocument;
using nenenib::application::SaveDocument;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::Selection;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::vim_step;
using nenenib::core::VimCharacter;
using nenenib::core::VimColumnWish;
using nenenib::core::VimEditorView;
using nenenib::core::VimKey;
using nenenib::core::VimSelect;
using nenenib::core::VimState;
using nenenib::core::VimViewport;
using nenenib::core::VirtualColumn;

void verify_vim_visual_wanted_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("visual-wanted-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 17, "the scope replays all 17 adopted visual-column fixtures");
}

void verify_vim_visual_wanted_continuations()
{
    for (const std::string_view keys :
         {"$vjj", "$Vjj", "$vVjj", "$Vvjj", "$vg<Esc>jj", "$vf<Esc>jj"})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(Bytes{std::string("abcd\nx\nabcdef")});
        applied(controller, VisibleLines{3});
        applied(controller, OpenDocument{sample_path()});
        applied(controller, SelectEditMode{EditMode::vim});
        vim_replay(controller, keys);
        expect(controller.frame().caret.position == TextPosition{LineNumber{3}, Column{7}},
               std::string(keys).c_str());
        expect(controller.vim_state().wanted_column ==
                   nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}},
               "visual transitions and waiting cancellation preserve the line-end wish");
        expect(vim_body(controller.frame()) == "abcd\nx\nabcdef",
               "visual transitions leave the document unchanged");
    }
}

void verify_vim_visual_wanted_counted()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nabcdef\nxy").value();
    const VimEditorView view{buffer, collapsed_at(Offset{3}), VimViewport{LineNumber{1}, 3}};
    VimState state = empty_vim_state();
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    state.count = nenenib::core::VimCount{2};
    const auto line = vim_step(state, view, VimKey{VimCharacter{U'V'}});
    expect(std::get<VimSelect>(line.effect).selection == Selection{Offset{3}, Offset{11}},
           "2V uses the inherited line-end wish on a longer destination line");
    expect(line.next.wanted_column == state.wanted_column && !line.next.count.has_value(),
           "2V preserves the wish but consumes its count");
    const auto character = vim_step(state, view, VimKey{VimCharacter{U'v'}});
    expect(character.next.wanted_column ==
               nenenib::core::VimWantedColumn{VimColumnWish::at_column, VirtualColumn{5}},
           "2v replaces the line-end wish with the widened endpoint column");
    state.count = nenenib::core::VimCount{1};
    const auto single = vim_step(state, view, VimKey{VimCharacter{U'v'}});
    expect(single.next.wanted_column == state.wanted_column,
           "explicit 1v preserves the wish without widening");
}

void verify_vim_visual_wanted_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("abcd\r\nx\r\nabcdef\r\nTAIL")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "$vjjd");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "abcTAIL", "the selection includes the endpoint newline");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "abcd\r\nx\r\nabcdef\r\nTAIL",
           "the existing undo restores the exact CRLF bytes");
}

void verify_vim_visual_wanted_blocked_expansion()
{
    const auto buffer = TextBuffer::from_utf8("\nabcdef").value();
    VimState state = empty_vim_state();
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    state.count = nenenib::core::VimCount{2};
    const VimEditorView empty{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 2}};
    const auto character = vim_step(state, empty, VimKey{VimCharacter{U'v'}});
    expect(character.next.wanted_column == state.wanted_column,
           "blocked horizontal expansion on an empty line preserves the wish");
    const auto down = vim_step(character.next, empty, VimKey{VimCharacter{U'j'}});
    expect(std::get<VimSelect>(down.effect).selection == Selection{Offset{0}, Offset{7}},
           "the next independent key reaches the longer line's NUL column");
    const VimEditorView last{buffer, collapsed_at(Offset{6}), VimViewport{LineNumber{1}, 2}};
    const auto line = vim_step(state, last, VimKey{VimCharacter{U'V'}});
    expect(std::get<VimSelect>(line.effect).selection == Selection{Offset{6}, Offset{6}},
           "2V at EOF does not move the caret to the NUL column");
    expect(line.next.wanted_column == state.wanted_column,
           "blocked vertical expansion preserves its wish");
}
} // namespace

void verify_vim_visual_wanted_contracts()
{
    verify_vim_visual_wanted_continuations();
    verify_vim_visual_wanted_counted();
    verify_vim_visual_wanted_blocked_expansion();
    verify_vim_visual_wanted_undo();
}

void verify_vim_visual_wanted_scope()
{
    verify_vim_visual_wanted_fixtures();
    verify_vim_visual_wanted_contracts();
    verify_vim_visual_step_edges();
}
} // namespace nenenib::tests
