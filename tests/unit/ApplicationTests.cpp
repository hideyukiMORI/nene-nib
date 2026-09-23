// application の単体テスト（ADR 0042 決定 3）。controller・文書・IME。
#include "Appearance.hpp"
#include "AppearanceReadFailure.hpp"
#include "BodyLayout.hpp"
#include "CancelComposition.hpp"
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "CaretShape.hpp"
#include "CaretView.hpp"
#include "ClauseEmphasis.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "CodePageFailure.hpp"
#include "Column.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "FileFailure.hpp"
#include "FontSize.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "ModeLabel.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "Palette.hpp"
#include "PlaceCaret.hpp"
#include "RefreshAppearance.hpp"
#include "RgbColor.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "ScrollLines.hpp"
#include "ScrollState.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::application::CancelComposition;
using nenenib::application::CancelSelection;
using nenenib::application::CaretView;
using nenenib::application::ClipboardAction;
using nenenib::application::ClipboardOperation;
using nenenib::application::CommitText;
using nenenib::application::ComposeText;
using nenenib::application::DeleteText;
using nenenib::application::EditorFrame;
using nenenib::application::EditorState;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::MoveCaret;
using nenenib::application::NewLine;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::RefreshAppearance;
using nenenib::application::SaveDocument;
using nenenib::application::ScrollLines;
using nenenib::application::ScrollState;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::body_layout;
using nenenib::core::byte_order_mark;
using nenenib::core::CaretMotion;
using nenenib::core::CaretShape;
using nenenib::core::ClauseEmphasis;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::composition_underlines;
using nenenib::core::CompositionClause;
using nenenib::core::DeleteDirection;
using nenenib::core::Edit;
using nenenib::core::EditBoundary;
using nenenib::core::EditHistory;
using nenenib::core::EditMode;
using nenenib::core::has_selection;
using nenenib::core::HistoryDirection;
using nenenib::core::LineEnding;
using nenenib::core::LineNumber;
using nenenib::core::mode_label;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::Palette;
using nenenib::core::palette_for;
using nenenib::core::RgbColor;
using nenenib::core::SaveState;
using nenenib::core::Selection;
using nenenib::core::SelectionAnchoring;
using nenenib::core::SelectionPresence;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::VimCharacter;
using nenenib::core::VimKey;
using nenenib::core::VimMode;

void verify_font_geometry()
{
    using nenenib::core::FontSize;
    const auto small = body_layout(640, 360, 96, FontSize::from_points(8).value());
    expect(small.line_height == 17 && small.gutter.right == 33,
           "eight points changes the row and gutter together");
    const auto large = body_layout(640, 360, 96, FontSize::from_points(40).value());
    expect(large.line_height == 85 && large.gutter.right == 166 && large.visible_lines == 3,
           "forty points is still measured in point-derived DIP");
    const auto double_dpi = body_layout(1280, 720, 192, FontSize::from_points(40).value());
    expect(double_dpi.line_height == 170 && double_dpi.gutter.right == 332 &&
               double_dpi.visible_lines == large.visible_lines,
           "DPI is applied once and preserves the visible line count");
    const auto narrow = body_layout(50, 360, 96, FontSize::from_points(40).value());
    expect(narrow.content.left == narrow.content.right, "a large gutter never inverts the content");
}

void verify_controller_initial_appearance()
{
    ScriptedAppearance light{Reading{Appearance::light}};
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    const EditorController from_light(
        nenenib::application::EditorPorts{light, board, files, code_pages, settings, themes});
    expect(from_light.frame().palette.background == RgbColor{0xF4, 0xF5, 0xF7},
           "a readable light setting is used");
    ScriptedAppearance dark{Reading{Appearance::dark}};
    const EditorController from_dark(
        nenenib::application::EditorPorts{dark, board, files, code_pages, settings, themes});
    expect(from_dark.frame().palette.background == RgbColor{0x30, 0x0A, 0x24},
           "a readable dark setting is used");
}

void verify_controller_read_failures()
{
    const Palette dark = palette_for(Appearance::dark);
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    for (const auto failure :
         {AppearanceReadFailure::unavailable, AppearanceReadFailure::unreadable})
    {
        ScriptedAppearance port{Reading{std::unexpect, failure}};
        const EditorController controller(
            nenenib::application::EditorPorts{port, board, files, code_pages, settings, themes});
        expect(controller.frame().palette.background == dark.background,
               "an unreadable setting falls back to dark");
    }
}

void verify_controller_refresh()
{
    ScriptedAppearance port{Reading{Appearance::light}};
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    EditorController controller(
        nenenib::application::EditorPorts{port, board, files, code_pages, settings, themes});
    port.script(Reading{Appearance::dark});
    const auto frame = controller.apply(RefreshAppearance{});
    expect(frame.palette.background == RgbColor{0x30, 0x0A, 0x24},
           "RefreshAppearance re-reads the port");
    expect(frame.appearance == Appearance::dark, "the frame carries the appearance for DWM");
    expect(controller.frame().palette.background == frame.palette.background,
           "the controller keeps the refreshed state");
}

void verify_controller_typing()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    expect(applied(controller, InsertText{"a"}) == "a", "a keystroke lands in the buffer");
    expect(applied(controller, InsertText{"b"}) == "ab", "the next keystroke follows");
    expect(applied(controller, NewLine{}) == "ab|", "Enter opens a new line");
    expect(applied(controller, InsertText{"c"}) == "ab|c", "typing continues on the new line");
    expect(applied(controller, InsertText{""}) == "ab|c", "an empty insert changes nothing");
    const auto frame = controller.frame();
    expect(frame.total_lines == 2, "the frame carries the total line count");
    expect(frame.caret.position == TextPosition{LineNumber{2}, Column{2}}, "the caret follows");
    expect(frame.status_items.at(0).text() == "行 2, 桁 2", "the status item follows the caret");
    expect(frame.caret.shape == CaretShape::bar, "the ordinary caret is a bar");
    expect(applied(controller, InsertText{"\xE6\x97\xA5"}) == "ab|c\xE6\x97\xA5",
           "a multibyte code point is one insert");
    expect(controller.frame().caret.position.column == Column{3}, "and one column");
}

void verify_controller_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    applied(controller, InsertText{"b"});
    applied(controller, InsertText{"c"});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "one undo removes the whole run of keystrokes");
    expect(applied(controller, HistoryAction{HistoryDirection::redo}) == "abc",
           "redo puts it back");
    expect(applied(controller, HistoryAction{HistoryDirection::redo}) == "abc",
           "redo at the tip changes nothing");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "undo at the start changes nothing");
    applied(controller, InsertText{"x"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"y"});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "x|",
           "typing after a newline is its own unit");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "x",
           "the newline is a unit of its own");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}},
           "undo puts the caret where the edit was");
}

void verify_controller_deletion()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "ab",
           "Backspace removes the code point before the caret");
    applied(controller, MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "ab",
           "Backspace at the start of the buffer does nothing");
    expect(applied(controller, DeleteText{DeleteDirection::forward}) == "b",
           "Delete removes the code point after the caret");
    applied(controller, MoveCaret{CaretMotion::document_end, SelectionAnchoring::collapse});
    expect(applied(controller, DeleteText{DeleteDirection::forward}) == "b",
           "Delete at the end of the buffer does nothing");
    applied(controller, NewLine{});
    applied(controller, InsertText{"c"});
    applied(controller, MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "bc",
           "Backspace at a line start removes the whole CRLF");
    applied(controller, InsertText{"\xE6\x97\xA5"});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "bc",
           "Backspace removes a whole multibyte code point");
}

void verify_controller_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abcd"});
    applied(controller, MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    applied(controller, MoveCaret{CaretMotion::next_character, SelectionAnchoring::extend});
    applied(controller, MoveCaret{CaretMotion::next_character, SelectionAnchoring::extend});
    const auto selected = controller.frame().lines.at(0).selection;
    expect(selected.presence == SelectionPresence::present, "Shift+Right selects");
    expect(selected.begin == Column{1} && selected.end == Column{3},
           "two code points are selected");
    expect(applied(controller, InsertText{"Z"}) == "Zcd", "typing over a selection replaces it");
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "the replacement collapses the selection");
    applied(controller, SelectAll{});
    expect(applied(controller, DeleteText{DeleteDirection::forward}).empty(),
           "Ctrl+A then Delete empties the buffer");
    applied(controller, InsertText{"ab"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"cd"});
    applied(controller, SelectAll{});
    const auto frame = controller.frame();
    expect(frame.lines.at(0).selection.end == Column{4},
           "a selection that crosses the line end reaches one column past the content");
    expect(frame.lines.at(1).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "the second line is selected to its content end");
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "moving without Shift collapses the selection");
}

void verify_controller_place_caret()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abcd"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"xy"});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{3}}, SelectionAnchoring::collapse});
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{3}},
           "a click puts the caret where it was asked for");
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "a plain click collapses the selection");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{2}, Column{2}}, SelectionAnchoring::extend});
    const auto frame = controller.frame();
    expect(frame.caret.position == TextPosition{LineNumber{2}, Column{2}},
           "Shift and a click move the caret");
    expect(frame.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{3}, Column{6}},
           "Shift and a click keep the anchor and select to the line end");
    expect(frame.lines.at(1).selection.begin == Column{1}, "the selection reaches the next line");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{9}, Column{99}}, SelectionAnchoring::collapse});
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{3}},
           "a position past the buffer clamps to the nearest one");
}

void verify_controller_cancel_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abcd"});
    applied(controller, SelectAll{});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::present,
           "the fixture has a selection");
    applied(controller, CancelSelection{});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "Esc drops the selection");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{5}},
           "Esc leaves the caret where it was");
    expect(applied(controller, CancelSelection{}) == "abcd",
           "Esc with nothing selected changes nothing");
    applied(controller, SelectAll{});
    applied(controller, SelectEditMode{EditMode::vim});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "entering Vim drops the selection and puts the caret on a character");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{4}},
           "and the caret does not stay past the last character");
    applied(controller, CancelSelection{});
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{4}},
           "in Vim mode Esc arrives as a VimKeyPress, so CancelSelection does nothing");
}

void verify_controller_clipboard()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"hello"});
    applied(controller, SelectAll{});
    applied(controller, ClipboardAction{ClipboardOperation::copy});
    expect(editing.clipboard().read().value() == "hello", "copy writes the selection");
    applied(controller, MoveCaret{CaretMotion::document_end, SelectionAnchoring::collapse});
    expect(applied(controller, ClipboardAction{ClipboardOperation::paste}) == "hellohello",
           "paste inserts at the caret");
    applied(controller, SelectAll{});
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}).empty(),
           "cut removes the selection");
    expect(editing.clipboard().read().value() == "hellohello", "cut wrote the text first");
    expect(applied(controller, ClipboardAction{ClipboardOperation::copy}).empty(),
           "copy with nothing selected changes nothing");
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}).empty(),
           "cut with nothing selected changes nothing");
}

void verify_controller_clipboard_failures()
{
    Editing refusing;
    refusing.clipboard().refuse_writes();
    EditorController &controller = refusing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    applied(controller, SelectAll{});
    expect(applied(controller, ClipboardAction{ClipboardOperation::copy}) == "abc",
           "a refused write leaves the text alone");
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}) == "abc",
           "a refused write does not cut the text away");
    Editing nothing_held;
    applied(nothing_held.controller(), VisibleLines{10});
    expect(applied(nothing_held.controller(), ClipboardAction{ClipboardOperation::paste}).empty(),
           "pasting an empty clipboard changes nothing");
    nothing_held.clipboard().hold(Content{"pasted"});
    expect(applied(nothing_held.controller(), ClipboardAction{ClipboardOperation::paste}) ==
               "pasted",
           "a clipboard filled from outside pastes");
}

void verify_controller_scrolling()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3});
    for (std::size_t line = 0; line < 9; ++line)
    {
        applied(controller, InsertText{"x"});
        applied(controller, NewLine{});
    }
    expect(controller.frame().total_lines == 10, "nine newlines make ten lines");
    expect(controller.frame().first_visible == LineNumber{8},
           "the caret on the last line pulls the window down");
    expect(controller.frame().lines.size() == 3, "three lines are visible");
    applied(controller, ScrollLines{-3});
    expect(controller.frame().first_visible == LineNumber{5}, "the wheel scrolls up three lines");
    applied(controller, ScrollLines{-99});
    expect(controller.frame().first_visible == LineNumber{1}, "scrolling up stops at the top");
    applied(controller, ScrollLines{99});
    expect(controller.frame().first_visible == LineNumber{8}, "scrolling down stops at the bottom");
    applied(controller, VisibleLines{0});
    expect(controller.frame().lines.size() == 1, "a window with no room still shows one line");
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    expect(controller.frame().first_visible == LineNumber{1}, "the caret pulls the window back up");
}

void verify_controller_frame()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto frame = controller.apply(VisibleLines{5});
    expect(frame.lines.size() == 1 && frame.lines.at(0).number == LineNumber{1},
           "an empty buffer shows one line");
    expect(frame.lines.at(0).text.empty(), "the only line is empty");
    expect(frame.lines.at(0).selection.presence == SelectionPresence::absent,
           "nothing is selected");
    expect(frame.document.title.text() == "無題", "the only tab is titled 無題");
    expect(frame.first_visible == LineNumber{1} && frame.total_lines == 1, "the window is at rest");
    expect(frame.status_items.at(2).text() == "CRLF", "a new buffer writes CRLF");
    expect(frame.caret == CaretView{TextPosition{LineNumber{1}, Column{1}}, CaretShape::bar},
           "the caret starts at the origin as a bar");
    const auto vim = controller.apply(SelectEditMode{EditMode::vim});
    expect(vim.caret.shape == CaretShape::block, "the Vim caret is a block");
    expect(!(vim.caret == frame.caret), "caret views compare on the shape too");
}

void verify_controller_mode_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.frame().mode == EditMode::ordinary, "the editor starts in ordinary mode");
    expect(controller.frame().mode_label == "通常", "the initial label is 通常");
    const auto vim = controller.apply(SelectEditMode{EditMode::vim});
    expect(vim.mode == EditMode::vim && vim.mode_label == "NORMAL", "select vim enters vim mode");
    const auto again = controller.apply(SelectEditMode{EditMode::vim});
    expect(again.mode == EditMode::vim, "selecting the mode already in force changes nothing");
    const auto ordinary = controller.apply(SelectEditMode{EditMode::ordinary});
    expect(ordinary.mode == EditMode::ordinary, "select ordinary returns to ordinary");
    expect(ordinary.palette.background == RgbColor{0x30, 0x0A, 0x24},
           "the palette survives the mode change");
}

void verify_document_open()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(std::string("一行目\r\n二行目"));
    const auto frame = controller.apply(OpenDocument{sample_path()});
    expect(editing.files().read_path() == "C:\\work\\note.txt", "the path went through the port");
    expect(editing.files().read_limit() == 64U * 1024U * 1024U,
           "the 64 MiB limit is handed to the port, not measured after the read");
    expect(frame.document.title.text() == "note.txt", "the tab takes the file name");
    expect(frame.document.encoding == TextEncoding::utf8, "plain UTF-8 is detected");
    expect(frame.document.save_state == SaveState::saved, "a freshly opened file is saved");
    expect(!frame.document.last_failure.has_value(), "opening left no failure behind");
    expect(frame.total_lines == 2, "both lines came in");
    expect(frame.lines.at(0).text == "一行目", "the first line is the first line of the file");
    expect(frame.caret.position == TextPosition{LineNumber{1}, Column{1}},
           "the caret starts at the top");
    expect(frame.first_visible == LineNumber{1}, "so does the window");
    expect(frame.status_items.at(1).text() == "UTF-8", "the status bar shows the encoding");
    expect(frame.status_items.at(2).text() == "CRLF", "and the line ending that was read");
}

void verify_document_open_encodings()
{
    Editing with_bom;
    with_bom.files().hold(std::string(byte_order_mark()) + "本文\nつづき");
    const auto bom = with_bom.controller().apply(OpenDocument{sample_path()});
    expect(bom.document.encoding == TextEncoding::utf8_bom, "the BOM is remembered");
    expect(bom.lines.at(0).text == "本文", "the BOM is not part of the body");
    expect(bom.status_items.at(2).text() == "LF", "an LF file keeps LF");
    Editing japanese;
    japanese.files().hold(std::string("\x93\xFA\x96\x7B"));
    japanese.code_pages().decode_to(std::string("日本"));
    const auto read = japanese.controller().apply(OpenDocument{sample_path()});
    expect(read.document.encoding == TextEncoding::shift_jis, "CP932 bytes open as Shift_JIS");
    expect(read.lines.at(0).text == "日本", "the port turned them into UTF-8");
    expect(read.status_items.at(1).text() == "Shift_JIS", "the status bar says so");
    // 開いてもモードは保たれる（決定 8）。
    Editing vim;
    vim.files().hold(std::string("x"));
    static_cast<void>(vim.controller().apply(SelectEditMode{EditMode::vim}));
    const auto kept = vim.controller().apply(OpenDocument{sample_path()});
    expect(kept.mode == EditMode::vim, "opening a file keeps the editing mode");
}

void expect_open_failure(Bytes content, FileFailure expected, const char *description)
{
    Editing editing;
    editing.files().hold(std::move(content));
    const auto frame = editing.controller().apply(OpenDocument{sample_path()});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == expected,
           description);
    expect(frame.document.title.text() == "無題", "a failed open does not change the tab");
    expect(frame.total_lines == 1 && frame.lines.at(0).text.empty(),
           "a failed open does not change the body");
}

void verify_document_open_failures()
{
    expect_open_failure(std::unexpected(FileFailure::not_found), FileFailure::not_found,
                        "a missing file is reported as not_found");
    expect_open_failure(std::unexpected(FileFailure::access_denied), FileFailure::access_denied,
                        "a refused file is reported as access_denied");
    expect_open_failure(std::unexpected(FileFailure::unreadable), FileFailure::unreadable,
                        "an unreadable file is reported as unreadable");
    expect_open_failure(std::unexpected(FileFailure::unwritable), FileFailure::unwritable,
                        "the port may also report unwritable");
    expect_open_failure(std::unexpected(FileFailure::too_large), FileFailure::too_large,
                        "a file past the limit is refused by the port before it is read");
    expect_open_failure(std::string("\xFF\xFE\x00\x41", 4), FileFailure::undecodable,
                        "bytes that are neither UTF-8 nor CP932 are undecodable");
    Editing refused;
    refused.files().hold(std::string("\x93\xFA"));
    refused.code_pages().decode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame = refused.controller().apply(OpenDocument{sample_path()});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == FileFailure::unencodable,
           "the code page port carries its own reason out");
    Editing broken;
    broken.files().hold(std::string("\x93\xFA"));
    broken.code_pages().decode_to(std::string("\xFF"));
    const auto invalid = broken.controller().apply(OpenDocument{sample_path()});
    expect(invalid.document.last_failure.has_value() &&
               invalid.document.last_failure.value() == FileFailure::undecodable,
           "a port that returns broken UTF-8 is undecodable");
}

void verify_document_save()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, InsertText{"あ"});
    expect(controller.frame().document.save_state == SaveState::modified,
           "typing marks it unsaved");
    expect(controller.frame().document.title.text() == "● 無題", "the mark is on the tab");
    const auto saved = controller.apply(SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "あ", "the body went to the port as UTF-8");
    expect(editing.files().written_path() == "C:\\work\\note.txt", "to the path in the intent");
    expect(saved.document.save_state == SaveState::saved, "saving clears the mark");
    expect(saved.document.title.text() == "note.txt", "and the tab takes the name");
    expect(saved.document.encoding == TextEncoding::utf8, "the encoding is the one that was asked");
    expect(saved.caret.position == TextPosition{LineNumber{1}, Column{2}},
           "saving does not move the caret");
    expect(!saved.document.last_failure.has_value(), "a good save reports nothing");
}

void verify_document_save_encodings()
{
    Editing with_bom;
    applied(with_bom.controller(), InsertText{"あ"});
    static_cast<void>(
        with_bom.controller().apply(SaveDocument{sample_path(), TextEncoding::utf8_bom}));
    expect(with_bom.files().written() == std::string(byte_order_mark()) + "あ",
           "a BOM file keeps its BOM");
    Editing japanese;
    applied(japanese.controller(), InsertText{"日"});
    japanese.code_pages().encode_to(std::string("\x93\xFA"));
    const auto frame =
        japanese.controller().apply(SaveDocument{sample_path(), TextEncoding::shift_jis});
    expect(japanese.code_pages().encoded_from() == "日", "the body went through the code page");
    expect(japanese.files().written() == "\x93\xFA", "the CP932 bytes are what is written");
    expect(frame.document.save_state == SaveState::saved, "the save took");
}

void verify_document_save_failures()
{
    Editing refused;
    applied(refused.controller(), InsertText{"😀"});
    refused.code_pages().encode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame =
        refused.controller().apply(SaveDocument{sample_path(), TextEncoding::shift_jis});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == FileFailure::unencodable,
           "a character CP932 cannot hold is unencodable");
    expect(frame.document.save_state == SaveState::modified, "the body is still unsaved");
    expect(!frame.document.path.has_value(), "the document did not adopt the path");
    expect(refused.files().written().empty(), "nothing was written");
    Editing unwritable;
    applied(unwritable.controller(), InsertText{"a"});
    unwritable.files().refuse_writes(FileFailure::unwritable);
    const auto failed =
        unwritable.controller().apply(SaveDocument{sample_path(), TextEncoding::utf8});
    expect(failed.document.last_failure.has_value() &&
               failed.document.last_failure.value() == FileFailure::unwritable,
           "a port that cannot write says so");
    expect(failed.document.save_state == SaveState::modified, "a failed save stays unsaved");
    expect(failed.document.title.text() == "● 無題", "and the tab does not take the name");
    expect(failed.lines.at(0).text == "a", "the body is untouched either way");
}

void verify_save_state_transitions()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.frame().document.save_state == SaveState::saved,
           "an untouched new buffer counts as saved");
    applied(controller, InsertText{"a"});
    expect(controller.frame().document.save_state == SaveState::modified, "one keystroke: unsaved");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::saved,
           "undo back to the start clears the mark");
    applied(controller, InsertText{"a"});
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(controller.frame().document.save_state == SaveState::saved, "saved again");
    // sealed() が無いと、この 1 打鍵が保存時点の単位に混ざって位置が動かない（決定 7）。
    applied(controller, InsertText{"b"});
    expect(controller.frame().document.save_state == SaveState::modified,
           "the keystroke after a save opens its own unit");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::saved,
           "undoing that keystroke returns to the save point");
}

void verify_unreachable_save_point()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, InsertText{"a"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"b"});
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    applied(controller, HistoryAction{HistoryDirection::undo});
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::modified,
           "undoing past the save point is unsaved");
    applied(controller, InsertText{"c"});
    applied(controller, HistoryAction{HistoryDirection::undo});
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::modified,
           "a new edit cut the redo list, so the save point is gone for good");
}

void verify_document_failure_clearing()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(std::unexpected(FileFailure::not_found));
    const auto failed = controller.apply(OpenDocument{sample_path()});
    expect(failed.document.last_failure.has_value(), "the failure is on the frame that made it");
    const auto next = controller.apply(InsertText{"a"});
    expect(!next.document.last_failure.has_value(), "the next intent clears the failure");
    expect(!controller.frame().document.last_failure.has_value(),
           "and reading the frame again does not bring it back");
}

[[nodiscard]] CompositionClause clause_of(std::size_t begin, std::size_t end,
                                          ClauseEmphasis emphasis)
{
    return CompositionClause{OffsetRange{Offset{begin}, Offset{end}}, emphasis};
}

// 変換中の表示値を 1 本の文字列にする。「utf8@キャレット|(T|O)開始-終了…」で、
// 変換していないときは "-"。optional は value() で読む（CPP-004）。
[[nodiscard]] std::string composed_summary(const EditorFrame &frame)
{
    if (!frame.composition.has_value())
    {
        return "-";
    }
    const auto &view = frame.composition.value();
    std::string summary = view.utf8 + "@" + std::to_string(view.cursor.value);
    for (const CompositionClause &clause : view.underlines)
    {
        summary += clause.emphasis == ClauseEmphasis::target ? "|T" : "|O";
        summary +=
            std::to_string(clause.range.begin.value) + "-" + std::to_string(clause.range.end.value);
    }
    return summary;
}

// 文節 → 下線の純関数（決定 7）。何が来ても変換中の文字列の全体を隙間なく覆う。
void verify_composition_underlines()
{
    expect(composition_underlines(composed_of("", {}, 0)).empty(),
           "an empty composition draws no underline");
    const auto plain = composition_underlines(composed_of("abcd", {}, 0));
    expect(plain.size() == 1 && plain.at(0) == clause_of(0, 4, ClauseEmphasis::other),
           "an IME that reports no clause still gets one underline over everything");
    const auto two = composition_underlines(composed_of(
        "abcd", {clause_of(0, 2, ClauseEmphasis::other), clause_of(2, 4, ClauseEmphasis::target)},
        2));
    expect(two.size() == 2 && two.at(1) == clause_of(2, 4, ClauseEmphasis::target),
           "clauses that already cover everything are kept as they are");
    const auto gapped =
        composition_underlines(composed_of("abcd", {clause_of(1, 2, ClauseEmphasis::target)}, 0));
    expect(gapped.size() == 3 && gapped.at(0) == clause_of(0, 1, ClauseEmphasis::other) &&
               gapped.at(1) == clause_of(1, 2, ClauseEmphasis::target) &&
               gapped.at(2) == clause_of(2, 4, ClauseEmphasis::other),
           "the gaps before and after a clause are covered as other");
    const auto clamped =
        composition_underlines(composed_of("abcd", {clause_of(0, 9, ClauseEmphasis::target)}, 0));
    expect(clamped.size() == 1 && clamped.at(0) == clause_of(0, 4, ClauseEmphasis::target),
           "a clause past the end of the composition is clamped to it");
    const auto empty_range =
        composition_underlines(composed_of("ab", {clause_of(1, 1, ClauseEmphasis::target)}, 0));
    expect(empty_range.size() == 2 && empty_range.at(0) == clause_of(0, 1, ClauseEmphasis::other) &&
               empty_range.at(1) == clause_of(1, 2, ClauseEmphasis::other),
           "an empty clause draws no underline of its own and the string stays covered");
    const auto reversed =
        composition_underlines(composed_of("ab", {clause_of(2, 1, ClauseEmphasis::target)}, 0));
    expect(reversed.size() == 1 && reversed.at(0) == clause_of(0, 2, ClauseEmphasis::other),
           "a reversed clause is dropped rather than drawn backwards");
    const auto overlap = composition_underlines(composed_of(
        "abcd", {clause_of(0, 3, ClauseEmphasis::target), clause_of(1, 4, ClauseEmphasis::other)},
        0));
    expect(overlap.size() == 2 && overlap.at(0) == clause_of(0, 3, ClauseEmphasis::target) &&
               overlap.at(1) == clause_of(3, 4, ClauseEmphasis::other),
           "overlapping clauses never draw two underlines over the same byte");
}

// 変換中は本文も履歴も動かない（ARC-004 / 決定 2）。EditorState の側で先に測る。
void verify_composition_state()
{
    const auto state = EditorState::create(Appearance::dark, EditMode::ordinary)
                           .with_edit(buffer_of("hi"), collapsed_at(Offset{2}),
                                      EditHistory::empty().pushed(Edit{Offset{0}, "", "hi"},
                                                                  EditBoundary::separate));
    expect(!state.composition().has_value(), "a state starts without a composition");
    const auto composing = state.with_composition(composed_of("あ", {}, 0));
    const auto &held = composing.composition();
    expect(held.has_value() && held.value().utf8 == "あ",
           "with_composition returns the next state");
    expect(!state.composition().has_value(), "with_composition leaves the source alone");
    expect(composing.text().text() == "hi", "the buffer does not change while composing");
    expect(composing.history().size() == state.history().size() &&
               composing.history().position() == state.history().position(),
           "the history does not change while composing");
    expect(composing.selection().caret == state.selection().caret,
           "the caret does not move while composing");
    expect(!composing.with_composition(std::nullopt).composition().has_value(),
           "with_composition also takes the composition away");
}

// 通常モード: 変換中は表示値にだけ載り、確定 1 回が 1 つの undo 単位になる（決定 4）。
void verify_composition_ordinary()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    const auto composing = controller.apply(ComposeText{composed_of(
        "にほん", {clause_of(0, 3, ClauseEmphasis::other), clause_of(3, 9, ClauseEmphasis::target)},
        3)});
    expect(composing.lines.at(0).text == "a", "the buffer does not carry the composed text");
    expect(composed_summary(composing) == "にほん@3|O0-3|T3-9",
           "the frame carries the composition, its caret and the folded underlines instead");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "",
           "undo while composing still only sees the typed 'a'");
    applied(controller, HistoryAction{HistoryDirection::redo});
    const auto committed = controller.apply(CommitText{"日本"});
    expect(committed.lines.at(0).text == "a日本", "the commit lands in the buffer");
    expect(!committed.composition.has_value(), "and the composition is gone");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "a",
           "one undo takes back the whole commit");
    expect(applied(controller, HistoryAction{HistoryDirection::redo}) == "a日本",
           "and redo puts it back");
}

// 変換をやめる 3 つの口: CancelComposition・モードの切り替え・ファイルを開く（決定 3）。
void verify_composition_cancelling()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto composing = controller.apply(ComposeText{composed_of("あ", {}, 0)});
    expect(composing.composition.has_value(), "the composition is on the frame");
    expect(!controller.apply(CancelComposition{}).composition.has_value(),
           "CancelComposition takes it away");
    expect(!controller.apply(CancelComposition{}).composition.has_value(),
           "and cancelling again does nothing");
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 0)}));
    expect(!controller.apply(SelectEditMode{EditMode::vim}).composition.has_value(),
           "changing the editing mode drops the composition");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 0)}));
    editing.files().hold(Bytes{std::string("x")});
    expect(!controller.apply(OpenDocument{sample_path()}).composition.has_value(),
           "opening a file drops the composition");
}

// Vim の NORMAL では IME を切ってあるので変換は来ないが、来たら捨てる（決定 4）。
void verify_composition_vim_normal()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    const auto composing = controller.apply(ComposeText{composed_of("に", {}, 0)});
    expect(!composing.composition.has_value(), "NORMAL drops the composition");
    expect(composing.vim_mode == VimMode::normal, "the frame carries the Vim mode for the window");
    const auto committed = controller.apply(CommitText{"日本"});
    expect(committed.lines.at(0).text == "abc", "NORMAL drops the committed text too");
    expect(controller.vim_state().mode == VimMode::normal, "and stays in NORMAL");
}

// Vim の INSERT では確定した文字列が code point ごとの打鍵として engine を通る（決定 4）。
void verify_composition_vim_insert()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'i'}}}));
    const auto composing = controller.apply(ComposeText{composed_of("にほん", {}, 9)});
    expect(composed_summary(composing) == "にほん@9|O0-9",
           "INSERT shows the composition like ordinary mode");
    expect(composing.vim_mode == VimMode::insert, "the frame says INSERT");
    expect(composing.lines.at(0).text.empty(), "the buffer is still empty while composing");
    const auto committed = controller.apply(CommitText{"日本語"});
    expect(committed.lines.at(0).text == "日本語", "the commit goes through the Vim engine");
    expect(!committed.composition.has_value(), "and the composition is gone");
    expect(controller.vim_state().mode == VimMode::insert, "INSERT is still INSERT afterwards");
    expect(committed.caret.position.column == Column{4}, "the caret sits past the three glyphs");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::escape}}));
    vim_replay(controller, "x");
    expect(controller.frame().lines.at(0).text == "日本",
           "Vim sees the committed text as characters it typed itself");
}

void verify_composition()
{
    verify_composition_underlines();
    verify_composition_state();
    verify_composition_ordinary();
    verify_composition_cancelling();
    verify_composition_vim_normal();
    verify_composition_vim_insert();
}
} // namespace

void verify_editor_state()
{
    const auto state = EditorState::create(Appearance::light, EditMode::ordinary);
    const auto next = state.with_appearance(Appearance::dark);
    expect(state.appearance() == Appearance::light, "with_appearance leaves the source alone");
    expect(next.appearance() == Appearance::dark, "with_appearance returns the next state");
    const auto switched = next.with_mode(EditMode::vim);
    expect(next.mode() == EditMode::ordinary, "with_mode leaves the source alone");
    expect(switched.mode() == EditMode::vim, "with_mode returns the next state");
    expect(switched.appearance() == Appearance::dark, "the appearance survives the mode change");
    expect(state.text().size_bytes() == 0, "a new state starts on an empty buffer");
    expect(state.line_ending() == LineEnding::crlf, "a new buffer writes CRLF");
    expect(state.history().size() == 0, "a new state has no history");
    expect(state.scroll() == ScrollState{LineNumber{1}, 1}, "a new state starts at the first line");
    const auto selected = state.with_selection(Selection{Offset{0}, Offset{0}});
    expect(!has_selection(selected.selection()), "with_selection keeps a collapsed selection");
    const auto scrolled = state.with_scroll(ScrollState{LineNumber{3}, 5});
    expect(scrolled.scroll() == ScrollState{LineNumber{3}, 5},
           "with_scroll returns the next state");
    expect(!(scrolled.scroll() == state.scroll()), "scroll states compare on both parts");
    const auto edited =
        state.with_edit(buffer_of("hi"), collapsed_at(Offset{2}), EditHistory::empty());
    expect(edited.text().text() == "hi" && state.text().size_bytes() == 0,
           "with_edit leaves the source alone");
}

void verify_controller_intents()
{
    verify_font_geometry();
    verify_controller_initial_appearance();
    verify_controller_read_failures();
    verify_controller_refresh();
    verify_controller_typing();
    verify_controller_undo();
    verify_controller_deletion();
    verify_controller_selection();
    verify_controller_place_caret();
    verify_controller_cancel_selection();
    verify_controller_clipboard();
    verify_controller_clipboard_failures();
    verify_controller_scrolling();
    verify_controller_frame();
    verify_controller_mode_selection();
    verify_document_open();
    verify_document_open_encodings();
    verify_document_open_failures();
    verify_document_save();
    verify_document_save_encodings();
    verify_document_save_failures();
    verify_save_state_transitions();
    verify_unreachable_save_point();
    verify_document_failure_clearing();
    verify_vim_engine();
    verify_composition();
}
} // namespace nenenib::tests
